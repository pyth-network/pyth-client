use std::borrow::BorrowMut;
use std::cell::RefMut;
use std::mem::{
    size_of,
    size_of_val,
};

use bytemuck::{
    bytes_of,
    bytes_of_mut,
};
use solana_program::account_info::AccountInfo;
use solana_program::clock::Clock;
use solana_program::entrypoint::SUCCESS;
use solana_program::program_error::ProgramError;
use solana_program::program_memory::{
    sol_memcpy,
    sol_memset,
};
use solana_program::pubkey::Pubkey;
use solana_program::rent::Rent;
use solana_program::sysvar::Sysvar;


use crate::time_machine_types::PriceAccountWrapper;
use solana_program::program::invoke;
use solana_program::system_instruction::transfer;
use solana_program::system_program::check_id;


use crate::c_oracle_header::{
    cmd_add_price_t,
    cmd_add_publisher_t,
    cmd_del_publisher_t,
    cmd_hdr_t,
    cmd_init_price_t,
    cmd_set_min_pub_t,
    cmd_upd_price_t,
    cmd_upd_product_t,
    command_t_e_cmd_upd_price,
    command_t_e_cmd_upd_price_no_fail_on_error,
    pc_acc,
    pc_ema_t,
    pc_map_table_t,
    pc_price_comp,
    pc_price_info_t,
    pc_price_t,
    pc_prod_t,
    pc_pub_key_t,
    PythAccount,
    PC_COMP_SIZE,
    PC_MAGIC,
    PC_MAP_TABLE_SIZE,
    PC_MAX_CI_DIVISOR,
    PC_MAX_NUM_DECIMALS,
    PC_PROD_ACC_SIZE,
    PC_PTYPE_UNKNOWN,
    PC_STATUS_UNKNOWN,
    PC_VERSION,
};
use crate::deserialize::{
    load,
    load_account_as,
    load_account_as_mut,
};
use crate::error::OracleResult;
use crate::OracleError;

use crate::utils::pyth_assert;

const PRICE_T_SIZE: usize = size_of::<pc_price_t>();
const PRICE_ACCOUNT_SIZE: usize = size_of::<PriceAccountWrapper>();


#[cfg(target_arch = "bpf")]
#[link(name = "cpyth")]
extern "C" {
    fn c_upd_aggregate(_input: *mut u8, clock_slot: u64, clock_timestamp: i64) -> bool;
}

#[cfg(not(target_arch = "bpf"))]
#[link(name = "cpythtest")]
extern "C" {
    fn c_upd_aggregate(_input: *mut u8, clock_slot: u64, clock_timestamp: i64) -> bool;
}

fn send_lamports<'a>(
    from: &AccountInfo<'a>,
    to: &AccountInfo<'a>,
    system_program: &AccountInfo<'a>,
    amount: u64,
) -> Result<(), ProgramError> {
    let transfer_instruction = transfer(from.key, to.key, amount);
    invoke(
        &transfer_instruction,
        &[from.clone(), to.clone(), system_program.clone()],
    )?;
    Ok(())
}

/// resizes a price account so that it fits the Time Machine
/// key[0] funding account       [signer writable]
/// key[1] price account         [Signer writable]
/// key[2] system program        [readable]
pub fn resize_price_account(
    program_id: &Pubkey,
    accounts: &[AccountInfo],
    _instruction_data: &[u8],
) -> OracleResult {
    let [funding_account_info, price_account_info, system_program] = match accounts {
        [x, y, z] => Ok([x, y, z]),
        _ => Err(ProgramError::InvalidArgument),
    }?;

    check_valid_funding_account(funding_account_info)?;
    check_valid_signable_account(program_id, price_account_info, size_of::<pc_price_t>())?;
    pyth_assert(
        check_id(system_program.key),
        OracleError::InvalidSystemAccount.into(),
    )?;
    //throw an error if not a price account
    //need to makre sure it goes out of scope immediatly to avoid mutable borrow errors
    {
        load_checked::<pc_price_t>(price_account_info, PC_VERSION)?;
    }
    let account_len = price_account_info.try_data_len()?;
    match account_len {
        PRICE_T_SIZE => {
            //ensure account is still rent exempt after resizing
            let rent: Rent = Default::default();
            let lamports_needed: u64 = rent
                .minimum_balance(size_of::<PriceAccountWrapper>())
                .saturating_sub(price_account_info.lamports());
            if lamports_needed > 0 {
                send_lamports(
                    funding_account_info,
                    price_account_info,
                    system_program,
                    lamports_needed,
                )?;
            }
            //resize
            //we do not need to zero initialize since this is the first time this memory
            //is allocated
            price_account_info.realloc(size_of::<PriceAccountWrapper>(), false)?;
            //The load below would fail if the account was not a price account, reverting the whole
            // transaction
            let mut price_account =
                load_checked::<PriceAccountWrapper>(price_account_info, PC_VERSION)?;
            //Initialize Time Machine
            price_account.initialize_time_machine()?;
            Ok(SUCCESS)
        }
        PRICE_ACCOUNT_SIZE => Ok(SUCCESS),
        _ => Err(ProgramError::InvalidArgument),
    }
}


/// initialize the first mapping account in a new linked-list of mapping accounts
/// accounts[0] funding account           [signer writable]
/// accounts[1] new mapping account       [signer writable]
pub fn init_mapping(
    program_id: &Pubkey,
    accounts: &[AccountInfo],
    instruction_data: &[u8],
) -> OracleResult {
    let [funding_account, fresh_mapping_account] = match accounts {
        [x, y] => Ok([x, y]),
        _ => Err(ProgramError::InvalidArgument),
    }?;

    check_valid_funding_account(funding_account)?;
    check_valid_signable_account(
        program_id,
        fresh_mapping_account,
        size_of::<pc_map_table_t>(),
    )?;
    check_valid_fresh_account(fresh_mapping_account)?;

    // Initialize by setting to zero again (just in case) and populating the account header
    let hdr = load::<cmd_hdr_t>(instruction_data)?;
    initialize_checked::<pc_map_table_t>(fresh_mapping_account, hdr.ver_)?;

    Ok(SUCCESS)
}

pub fn add_mapping(
    program_id: &Pubkey,
    accounts: &[AccountInfo],
    instruction_data: &[u8],
) -> OracleResult {
    let [funding_account, cur_mapping, next_mapping] = match accounts {
        [x, y, z] => Ok([x, y, z]),
        _ => Err(ProgramError::InvalidArgument),
    }?;

    check_valid_funding_account(funding_account)?;
    check_valid_signable_account(program_id, cur_mapping, size_of::<pc_map_table_t>())?;
    check_valid_signable_account(program_id, next_mapping, size_of::<pc_map_table_t>())?;
    check_valid_fresh_account(next_mapping)?;

    let hdr = load::<cmd_hdr_t>(instruction_data)?;
    let mut cur_mapping = load_checked::<pc_map_table_t>(cur_mapping, hdr.ver_)?;
    pyth_assert(
        cur_mapping.num_ == PC_MAP_TABLE_SIZE && pubkey_is_zero(&cur_mapping.next_),
        ProgramError::InvalidArgument,
    )?;

    initialize_checked::<pc_map_table_t>(next_mapping, hdr.ver_)?;
    pubkey_assign(&mut cur_mapping.next_, &next_mapping.key.to_bytes());

    Ok(SUCCESS)
}

/// a publisher updates a price
/// accounts[0] publisher account                                   [signer writable]
/// accounts[1] price account to update                             [writable]
/// accounts[2] sysvar clock                                        []
pub fn upd_price(
    program_id: &Pubkey,
    accounts: &[AccountInfo],
    instruction_data: &[u8],
) -> OracleResult {
    let cmd_args = load::<cmd_upd_price_t>(instruction_data)?;

    let [funding_account, price_account, clock_account] = match accounts {
        [x, y, z] => Ok([x, y, z]),
        [x, y, _, z] => Ok([x, y, z]),
        _ => Err(ProgramError::InvalidArgument),
    }?;

    check_valid_funding_account(funding_account)?;
    check_valid_writable_account(program_id, price_account, size_of::<pc_price_t>())?;
    // Check clock
    let clock = Clock::from_account_info(clock_account)?;

    let mut publisher_index: usize = 0;
    let latest_aggregate_price: pc_price_info_t;
    let latest_publisher_price: pc_price_info_t;
    {
        // Verify that symbol account is initialized
        let price_data = load_checked::<pc_price_t>(price_account, cmd_args.ver_)?;

        // Verify that publisher is valid
        while publisher_index < price_data.num_ as usize {
            if pubkey_equal(
                &price_data.comp_[publisher_index].pub_,
                &funding_account.key.to_bytes(),
            ) {
                break;
            }
            publisher_index += 1;
        }
        pyth_assert(
            publisher_index < price_data.num_ as usize,
            ProgramError::InvalidArgument,
        )?;


        latest_publisher_price = price_data.comp_[publisher_index].latest_;
        latest_aggregate_price = price_data.agg_;

        // Check that publisher is publishing a more recent price
        pyth_assert(
            !is_component_update(cmd_args)?
                || cmd_args.pub_slot_ > latest_publisher_price.pub_slot_,
            ProgramError::InvalidArgument,
        )?;
    }

    // Try to update the aggregate
    let mut aggregate_updated = false;
    if clock.slot > latest_aggregate_price.pub_slot_ {
        unsafe {
            aggregate_updated = c_upd_aggregate(
                price_account.try_borrow_mut_data()?.as_mut_ptr(),
                clock.slot,
                clock.unix_timestamp,
            );
        }
    }

    // Try to update the publisher's price
    if is_component_update(cmd_args)? {
        let mut status: u32 = cmd_args.status_;
        let mut threshold_conf = cmd_args.price_ / PC_MAX_CI_DIVISOR as i64;

        if threshold_conf < 0 {
            threshold_conf = -threshold_conf;
        }

        if cmd_args.conf_ > try_convert::<_, u64>(threshold_conf)? {
            status = PC_STATUS_UNKNOWN
        }

        {
            let mut price_data = load_checked::<pc_price_t>(price_account, cmd_args.ver_)?;
            let publisher_price = &mut price_data.comp_[publisher_index].latest_;
            publisher_price.price_ = cmd_args.price_;
            publisher_price.conf_ = cmd_args.conf_;
            publisher_price.status_ = status;
            publisher_price.pub_slot_ = cmd_args.pub_slot_;
        }
    }

    let account_len = price_account.try_data_len()?;
    if aggregate_updated && account_len == PRICE_ACCOUNT_SIZE {
        let mut price_account = load_account_as_mut::<PriceAccountWrapper>(price_account)?;
        price_account.add_price_to_time_machine()?;
    }

    Ok(SUCCESS)
}

pub fn upd_price_no_fail(
    program_id: &Pubkey,
    accounts: &[AccountInfo],
    instruction_data: &[u8],
) -> OracleResult {
    match upd_price(program_id, accounts, instruction_data) {
        Err(_) => Ok(SUCCESS),
        Ok(value) => Ok(value),
    }
}


/// add a price account to a product account
/// accounts[0] funding account                                   [signer writable]
/// accounts[1] product account to add the price account to       [signer writable]
/// accounts[2] newly created price account                       [signer writable]
pub fn add_price(
    program_id: &Pubkey,
    accounts: &[AccountInfo],
    instruction_data: &[u8],
) -> OracleResult {
    let cmd_args = load::<cmd_add_price_t>(instruction_data)?;

    check_exponent_range(cmd_args.expo_)?;
    pyth_assert(
        cmd_args.ptype_ != PC_PTYPE_UNKNOWN,
        ProgramError::InvalidArgument,
    )?;


    let [funding_account, product_account, price_account] = match accounts {
        [x, y, z] => Ok([x, y, z]),
        _ => Err(ProgramError::InvalidArgument),
    }?;

    check_valid_funding_account(funding_account)?;
    check_valid_signable_account(program_id, product_account, PC_PROD_ACC_SIZE as usize)?;
    check_valid_signable_account(program_id, price_account, size_of::<pc_price_t>())?;
    check_valid_fresh_account(price_account)?;

    let mut product_data = load_checked::<pc_prod_t>(product_account, cmd_args.ver_)?;

    let mut price_data = initialize_checked::<pc_price_t>(price_account, cmd_args.ver_)?;
    price_data.expo_ = cmd_args.expo_;
    price_data.ptype_ = cmd_args.ptype_;
    pubkey_assign(&mut price_data.prod_, &product_account.key.to_bytes());
    pubkey_assign(&mut price_data.next_, bytes_of(&product_data.px_acc_));
    pubkey_assign(&mut product_data.px_acc_, &price_account.key.to_bytes());

    Ok(SUCCESS)
}

pub fn init_price(
    program_id: &Pubkey,
    accounts: &[AccountInfo],
    instruction_data: &[u8],
) -> OracleResult {
    let cmd_args = load::<cmd_init_price_t>(instruction_data)?;

    check_exponent_range(cmd_args.expo_)?;

    let [funding_account, price_account] = match accounts {
        [x, y] => Ok([x, y]),
        _ => Err(ProgramError::InvalidArgument),
    }?;

    check_valid_funding_account(funding_account)?;
    check_valid_signable_account(program_id, price_account, size_of::<pc_price_t>())?;

    let mut price_data = load_checked::<pc_price_t>(price_account, cmd_args.ver_)?;
    pyth_assert(
        price_data.ptype_ == cmd_args.ptype_,
        ProgramError::InvalidArgument,
    )?;

    price_data.expo_ = cmd_args.expo_;

    price_data.last_slot_ = 0;
    price_data.valid_slot_ = 0;
    price_data.agg_.pub_slot_ = 0;
    price_data.prev_slot_ = 0;
    price_data.prev_price_ = 0;
    price_data.prev_conf_ = 0;
    price_data.prev_timestamp_ = 0;
    sol_memset(
        bytes_of_mut(&mut price_data.twap_),
        0,
        size_of::<pc_ema_t>(),
    );
    sol_memset(
        bytes_of_mut(&mut price_data.twac_),
        0,
        size_of::<pc_ema_t>(),
    );
    sol_memset(
        bytes_of_mut(&mut price_data.agg_),
        0,
        size_of::<pc_price_info_t>(),
    );
    for i in 0..(price_data.comp_.len() as usize) {
        sol_memset(
            bytes_of_mut(&mut price_data.comp_[i].agg_),
            0,
            size_of::<pc_price_info_t>(),
        );
        sol_memset(
            bytes_of_mut(&mut price_data.comp_[i].latest_),
            0,
            size_of::<pc_price_info_t>(),
        );
    }

    Ok(SUCCESS)
}

/// add a publisher to a price account
/// accounts[0] funding account                                   [signer writable]
/// accounts[1] price account to add the publisher to             [signer writable]
pub fn add_publisher(
    program_id: &Pubkey,
    accounts: &[AccountInfo],
    instruction_data: &[u8],
) -> OracleResult {
    let cmd_args = load::<cmd_add_publisher_t>(instruction_data)?;

    pyth_assert(
        instruction_data.len() == size_of::<cmd_add_publisher_t>()
            && !pubkey_is_zero(&cmd_args.pub_),
        ProgramError::InvalidArgument,
    )?;

    let [funding_account, price_account] = match accounts {
        [x, y] => Ok([x, y]),
        _ => Err(ProgramError::InvalidArgument),
    }?;

    check_valid_funding_account(funding_account)?;
    check_valid_signable_account(program_id, price_account, size_of::<pc_price_t>())?;

    let mut price_data = load_checked::<pc_price_t>(price_account, cmd_args.ver_)?;

    if price_data.num_ >= PC_COMP_SIZE {
        return Err(ProgramError::InvalidArgument);
    }

    for i in 0..(price_data.num_ as usize) {
        if pubkey_equal(&cmd_args.pub_, bytes_of(&price_data.comp_[i].pub_)) {
            return Err(ProgramError::InvalidArgument);
        }
    }

    let current_index: usize = try_convert(price_data.num_)?;
    sol_memset(
        bytes_of_mut(&mut price_data.comp_[current_index]),
        0,
        size_of::<pc_price_comp>(),
    );
    pubkey_assign(
        &mut price_data.comp_[current_index].pub_,
        bytes_of(&cmd_args.pub_),
    );
    price_data.num_ += 1;
    price_data.size_ =
        try_convert::<_, u32>(size_of::<pc_price_t>() - size_of_val(&price_data.comp_))?
            + price_data.num_ * try_convert::<_, u32>(size_of::<pc_price_comp>())?;
    Ok(SUCCESS)
}

/// add a publisher to a price account
/// accounts[0] funding account                                   [signer writable]
/// accounts[1] price account to delete the publisher from        [signer writable]
pub fn del_publisher(
    program_id: &Pubkey,
    accounts: &[AccountInfo],
    instruction_data: &[u8],
) -> OracleResult {
    let cmd_args = load::<cmd_del_publisher_t>(instruction_data)?;

    pyth_assert(
        instruction_data.len() == size_of::<cmd_del_publisher_t>()
            && !pubkey_is_zero(&cmd_args.pub_),
        ProgramError::InvalidArgument,
    )?;

    let [funding_account, price_account] = match accounts {
        [x, y] => Ok([x, y]),
        _ => Err(ProgramError::InvalidArgument),
    }?;

    check_valid_funding_account(funding_account)?;
    check_valid_signable_account(program_id, price_account, size_of::<pc_price_t>())?;

    let mut price_data = load_checked::<pc_price_t>(price_account, cmd_args.ver_)?;

    for i in 0..(price_data.num_ as usize) {
        if pubkey_equal(&cmd_args.pub_, bytes_of(&price_data.comp_[i].pub_)) {
            for j in i + 1..(price_data.num_ as usize) {
                price_data.comp_[j - 1] = price_data.comp_[j];
            }
            price_data.num_ -= 1;
            let current_index: usize = try_convert(price_data.num_)?;
            sol_memset(
                bytes_of_mut(&mut price_data.comp_[current_index]),
                0,
                size_of::<pc_price_comp>(),
            );
            price_data.size_ =
                try_convert::<_, u32>(size_of::<pc_price_t>() - size_of_val(&price_data.comp_))?
                    + price_data.num_ * try_convert::<_, u32>(size_of::<pc_price_comp>())?;
            return Ok(SUCCESS);
        }
    }
    Err(ProgramError::InvalidArgument)
}

pub fn add_product(
    program_id: &Pubkey,
    accounts: &[AccountInfo],
    instruction_data: &[u8],
) -> OracleResult {
    let [funding_account, tail_mapping_account, new_product_account] = match accounts {
        [x, y, z] => Ok([x, y, z]),
        _ => Err(ProgramError::InvalidArgument),
    }?;

    check_valid_funding_account(funding_account)?;
    check_valid_signable_account(
        program_id,
        tail_mapping_account,
        size_of::<pc_map_table_t>(),
    )?;
    check_valid_signable_account(program_id, new_product_account, PC_PROD_ACC_SIZE as usize)?;
    check_valid_fresh_account(new_product_account)?;

    let hdr = load::<cmd_hdr_t>(instruction_data)?;
    let mut mapping_data = load_checked::<pc_map_table_t>(tail_mapping_account, hdr.ver_)?;
    // The mapping account must have free space to add the product account
    pyth_assert(
        mapping_data.num_ < PC_MAP_TABLE_SIZE,
        ProgramError::InvalidArgument,
    )?;

    initialize_checked::<pc_prod_t>(new_product_account, hdr.ver_)?;

    let current_index: usize = try_convert(mapping_data.num_)?;
    pubkey_assign(
        &mut mapping_data.prod_[current_index],
        bytes_of(&new_product_account.key.to_bytes()),
    );
    mapping_data.num_ += 1;
    mapping_data.size_ =
        try_convert::<_, u32>(size_of::<pc_map_table_t>() - size_of_val(&mapping_data.prod_))?
            + mapping_data.num_ * try_convert::<_, u32>(size_of::<pc_pub_key_t>())?;

    Ok(SUCCESS)
}

/// Update the metadata associated with a product, overwriting any existing metadata.
/// The metadata is provided as a list of key-value pairs at the end of the `instruction_data`.
pub fn upd_product(
    program_id: &Pubkey,
    accounts: &[AccountInfo],
    instruction_data: &[u8],
) -> OracleResult {
    let [funding_account, product_account] = match accounts {
        [x, y] => Ok([x, y]),
        _ => Err(ProgramError::InvalidArgument),
    }?;

    check_valid_funding_account(funding_account)?;
    check_valid_signable_account(program_id, product_account, try_convert(PC_PROD_ACC_SIZE)?)?;

    let hdr = load::<cmd_hdr_t>(instruction_data)?;
    {
        // Validate that product_account contains the appropriate account header
        let mut _product_data = load_checked::<pc_prod_t>(product_account, hdr.ver_)?;
    }

    pyth_assert(
        instruction_data.len() >= size_of::<cmd_upd_product_t>(),
        ProgramError::InvalidInstructionData,
    )?;
    let new_data_len = instruction_data.len() - size_of::<cmd_upd_product_t>();
    let max_data_len = try_convert::<_, usize>(PC_PROD_ACC_SIZE)? - size_of::<pc_prod_t>();
    pyth_assert(new_data_len <= max_data_len, ProgramError::InvalidArgument)?;

    let new_data = &instruction_data[size_of::<cmd_upd_product_t>()..instruction_data.len()];
    let mut idx = 0;
    // new_data must be a list of key-value pairs, both of which are instances of pc_str_t.
    // Try reading the key-value pairs to validate that new_data is properly formatted.
    while idx < new_data.len() {
        let key = read_pc_str_t(&new_data[idx..])?;
        idx += key.len();
        let value = read_pc_str_t(&new_data[idx..])?;
        idx += value.len();
    }

    // This assertion shouldn't ever fail, but be defensive.
    pyth_assert(idx == new_data.len(), ProgramError::InvalidArgument)?;

    {
        let mut data = product_account.try_borrow_mut_data()?;
        // Note that this memcpy doesn't necessarily overwrite all existing data in the account.
        // This case is handled by updating the .size_ field below.
        sol_memcpy(
            &mut data[size_of::<pc_prod_t>()..],
            new_data,
            new_data.len(),
        );
    }

    let mut product_data = load_checked::<pc_prod_t>(product_account, hdr.ver_)?;
    product_data.size_ = try_convert(size_of::<pc_prod_t>() + new_data.len())?;

    Ok(SUCCESS)
}

pub fn set_min_pub(
    program_id: &Pubkey,
    accounts: &[AccountInfo],
    instruction_data: &[u8],
) -> OracleResult {
    let cmd = load::<cmd_set_min_pub_t>(instruction_data)?;

    pyth_assert(
        instruction_data.len() == size_of::<cmd_set_min_pub_t>(),
        ProgramError::InvalidArgument,
    )?;

    let [funding_account, price_account] = match accounts {
        [x, y] => Ok([x, y]),
        _ => Err(ProgramError::InvalidArgument),
    }?;

    check_valid_funding_account(funding_account)?;
    check_valid_signable_account(program_id, price_account, size_of::<pc_price_t>())?;

    let mut price_account_data = load_checked::<pc_price_t>(price_account, cmd.ver_)?;
    price_account_data.min_pub_ = cmd.min_pub_;

    Ok(SUCCESS)
}

fn valid_funding_account(account: &AccountInfo) -> bool {
    account.is_signer && account.is_writable
}

fn check_valid_funding_account(account: &AccountInfo) -> Result<(), ProgramError> {
    pyth_assert(
        valid_funding_account(account),
        OracleError::InvalidFundingAccount.into(),
    )
}

fn valid_writable_account(program_id: &Pubkey, account: &AccountInfo, minimum_size: usize) -> bool {
    account.is_writable
        && account.owner == program_id
        && account.data_len() >= minimum_size
        && Rent::default().is_exempt(account.lamports(), account.data_len())
}

fn check_valid_writable_account(
    program_id: &Pubkey,
    account: &AccountInfo,
    minimum_size: usize,
) -> Result<(), ProgramError> {
    pyth_assert(
        valid_writable_account(program_id, account, minimum_size),
        OracleError::InvalidSignableAccount.into(),
    )
}

fn valid_signable_account(program_id: &Pubkey, account: &AccountInfo, minimum_size: usize) -> bool {
    account.is_signer
        && account.is_writable
        && account.owner == program_id
        && account.data_len() >= minimum_size
        && Rent::default().is_exempt(account.lamports(), account.data_len())
}

fn check_valid_signable_account(
    program_id: &Pubkey,
    account: &AccountInfo,
    minimum_size: usize,
) -> Result<(), ProgramError> {
    pyth_assert(
        valid_signable_account(program_id, account, minimum_size),
        OracleError::InvalidSignableAccount.into(),
    )
}

/// Returns `true` if the `account` is fresh, i.e., its data can be overwritten.
/// Use this check to prevent accidentally overwriting accounts whose data is already populated.
fn valid_fresh_account(account: &AccountInfo) -> bool {
    let pyth_acc = load_account_as::<pc_acc>(account);
    match pyth_acc {
        Ok(pyth_acc) => pyth_acc.magic_ == 0 && pyth_acc.ver_ == 0,
        Err(_) => false,
    }
}

fn check_valid_fresh_account(account: &AccountInfo) -> Result<(), ProgramError> {
    pyth_assert(
        valid_fresh_account(account),
        OracleError::InvalidFreshAccount.into(),
    )
}

// Check that an exponent is within the range of permitted exponents for price accounts.
fn check_exponent_range(expo: i32) -> Result<(), ProgramError> {
    pyth_assert(
        expo >= -(PC_MAX_NUM_DECIMALS as i32) && expo <= PC_MAX_NUM_DECIMALS as i32,
        ProgramError::InvalidArgument,
    )
}

/// Sets the data of account to all-zero
pub fn clear_account(account: &AccountInfo) -> Result<(), ProgramError> {
    let mut data = account
        .try_borrow_mut_data()
        .map_err(|_| ProgramError::InvalidArgument)?;
    let length = data.len();
    sol_memset(data.borrow_mut(), 0, length);
    Ok(())
}

pub fn load_checked<'a, T: PythAccount>(
    account: &'a AccountInfo,
    version: u32,
) -> Result<RefMut<'a, T>, ProgramError> {
    {
        let account_header = load_account_as::<pc_acc>(account)?;
        pyth_assert(
            account_header.magic_ == PC_MAGIC
                && account_header.ver_ == version
                && account_header.type_ == T::ACCOUNT_TYPE,
            ProgramError::InvalidArgument,
        )?;
    }

    load_account_as_mut::<T>(account)
}

pub fn initialize_checked<'a, T: PythAccount>(
    account: &'a AccountInfo,
    version: u32,
) -> Result<RefMut<'a, T>, ProgramError> {
    clear_account(account)?;

    {
        let mut account_header = load_account_as_mut::<pc_acc>(account)?;
        account_header.magic_ = PC_MAGIC;
        account_header.ver_ = version;
        account_header.type_ = T::ACCOUNT_TYPE;
        account_header.size_ = T::INITIAL_SIZE;
    }

    load_account_as_mut::<T>(account)
}

// Assign pubkey bytes from source to target, fails if source is not 32 bytes
pub fn pubkey_assign(target: &mut pc_pub_key_t, source: &[u8]) {
    unsafe { target.k1_.copy_from_slice(source) }
}

pub fn pubkey_is_zero(key: &pc_pub_key_t) -> bool {
    return unsafe { key.k8_.iter().all(|x| *x == 0) };
}

pub fn pubkey_equal(target: &pc_pub_key_t, source: &[u8]) -> bool {
    unsafe { target.k1_ == *source }
}

/// Convert `x: T` into a `U`, returning the appropriate `OracleError` if the conversion fails.
pub fn try_convert<T, U: TryFrom<T>>(x: T) -> Result<U, OracleError> {
    // Note: the error here assumes we're only applying this function to integers right now.
    U::try_from(x).map_err(|_| OracleError::IntegerCastingError)
}

pub fn is_component_update(cmd_args: &cmd_upd_price_t) -> Result<bool, ProgramError> {
    Ok(
        try_convert::<_, u32>(cmd_args.cmd_)? == command_t_e_cmd_upd_price
            || try_convert::<_, u32>(cmd_args.cmd_)? == command_t_e_cmd_upd_price_no_fail_on_error,
    )
}

/// Read a `pc_str_t` from the beginning of `source`. Returns a slice of `source` containing
/// the bytes of the `pc_str_t`.
pub fn read_pc_str_t(source: &[u8]) -> Result<&[u8], ProgramError> {
    if source.is_empty() {
        Err(ProgramError::InvalidArgument)
    } else {
        let tag_len: usize = try_convert(source[0])?;
        if tag_len + 1 > source.len() {
            Err(ProgramError::InvalidArgument)
        } else {
            Ok(&source[..(1 + tag_len)])
        }
    }
}
