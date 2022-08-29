use std::mem::{
    size_of,
    size_of_val,
};

use crate::c_oracle_header::{
    CPubkey,
    MappingAccount,
    PriceAccount,
    PriceComponent,
    PriceEma,
    PriceInfo,
    ProductAccount,
    PC_COMP_SIZE,
    PC_MAP_TABLE_SIZE,
    PC_MAX_CI_DIVISOR,
    PC_PROD_ACC_SIZE,
    PC_PTYPE_UNKNOWN,
    PC_STATUS_UNKNOWN,
    PC_VERSION,
};
use crate::deserialize::{
    initialize_pyth_account_checked,
    load,
    load_account_as_mut,
    load_checked,
};
use crate::instruction::{
    AddPriceArgs,
    AddPublisherArgs,
    CommandHeader,
    DelPublisherArgs,
    InitPriceArgs,
    SetMinPubArgs,
    UpdPriceArgs,
};
use crate::time_machine_types::PriceAccountWrapper;
use crate::utils::{
    check_exponent_range,
    check_valid_funding_account,
    check_valid_signable_account,
    check_valid_writable_account,
    is_component_update,
    pubkey_assign,
    pubkey_clear,
    pubkey_equal,
    pubkey_is_zero,
    pyth_assert,
    read_pc_str_t,
    try_convert,
};
use crate::OracleError;
use bytemuck::{
    bytes_of,
    bytes_of_mut,
};
use solana_program::account_info::AccountInfo;
use solana_program::clock::Clock;
use solana_program::entrypoint::ProgramResult;
use solana_program::program::invoke;
use solana_program::program_error::ProgramError;
use solana_program::program_memory::{
    sol_memcpy,
    sol_memset,
};
use solana_program::pubkey::Pubkey;
use solana_program::rent::Rent;
use solana_program::system_instruction::transfer;
use solana_program::system_program::check_id;
use solana_program::sysvar::Sysvar;

const PRICE_T_SIZE: usize = size_of::<PriceAccount>();
const PRICE_ACCOUNT_SIZE: usize = size_of::<PriceAccountWrapper>();


#[cfg(target_arch = "bpf")]
#[link(name = "cpyth-bpf")]
extern "C" {
    pub fn c_upd_aggregate(_input: *mut u8, clock_slot: u64, clock_timestamp: i64) -> bool;
}

#[cfg(not(target_arch = "bpf"))]
#[link(name = "cpyth-native")]
extern "C" {
    pub fn c_upd_aggregate(_input: *mut u8, clock_slot: u64, clock_timestamp: i64) -> bool;
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
) -> ProgramResult {
    let [funding_account_info, price_account_info, system_program] = match accounts {
        [x, y, z] => Ok([x, y, z]),
        _ => Err(ProgramError::InvalidArgument),
    }?;

    check_valid_funding_account(funding_account_info)?;
    check_valid_signable_account(program_id, price_account_info)?;
    pyth_assert(
        check_id(system_program.key),
        OracleError::InvalidSystemAccount.into(),
    )?;
    // Check that it is a valid initialized price account
    {
        load_checked::<PriceAccount>(price_account_info, PC_VERSION)?;
    }
    let account_len = price_account_info.try_data_len()?;
    match account_len {
        PRICE_T_SIZE => {
            // Ensure account is still rent exempt after resizing
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
            // We do not need to zero allocate because we won't access the data in the same
            // instruction
            price_account_info.realloc(size_of::<PriceAccountWrapper>(), false)?;

            // Check that everything is ok
            check_valid_signable_account(program_id, price_account_info)?;
            let mut price_account =
                load_checked::<PriceAccountWrapper>(price_account_info, PC_VERSION)?;
            // Initialize Time Machine
            price_account.initialize_time_machine()?;
            Ok(())
        }
        PRICE_ACCOUNT_SIZE => Ok(()),
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
) -> ProgramResult {
    let [funding_account, fresh_mapping_account] = match accounts {
        [x, y] => Ok([x, y]),
        _ => Err(ProgramError::InvalidArgument),
    }?;

    check_valid_funding_account(funding_account)?;
    check_valid_signable_account(program_id, fresh_mapping_account)?;

    // Initialize by setting to zero again (just in case) and populating the account header
    let hdr = load::<CommandHeader>(instruction_data)?;
    initialize_pyth_account_checked::<MappingAccount>(fresh_mapping_account, hdr.version)?;

    Ok(())
}

pub fn add_mapping(
    program_id: &Pubkey,
    accounts: &[AccountInfo],
    instruction_data: &[u8],
) -> ProgramResult {
    let [funding_account, cur_mapping, next_mapping] = match accounts {
        [x, y, z] => Ok([x, y, z]),
        _ => Err(ProgramError::InvalidArgument),
    }?;

    check_valid_funding_account(funding_account)?;
    check_valid_signable_account(program_id, cur_mapping)?;
    check_valid_signable_account(program_id, next_mapping)?;

    let hdr = load::<CommandHeader>(instruction_data)?;
    let mut cur_mapping = load_checked::<MappingAccount>(cur_mapping, hdr.version)?;
    pyth_assert(
        cur_mapping.number_of_products == PC_MAP_TABLE_SIZE
            && pubkey_is_zero(&cur_mapping.next_mapping_account),
        ProgramError::InvalidArgument,
    )?;

    initialize_pyth_account_checked::<MappingAccount>(next_mapping, hdr.version)?;
    pubkey_assign(
        &mut cur_mapping.next_mapping_account,
        &next_mapping.key.to_bytes(),
    );

    Ok(())
}

/// a publisher updates a price
/// accounts[0] publisher account                                   [signer writable]
/// accounts[1] price account to update                             [writable]
/// accounts[2] sysvar clock                                        []
pub fn upd_price(
    program_id: &Pubkey,
    accounts: &[AccountInfo],
    instruction_data: &[u8],
) -> ProgramResult {
    let cmd_args = load::<UpdPriceArgs>(instruction_data)?;

    let [funding_account, price_account, clock_account] = match accounts {
        [x, y, z] => Ok([x, y, z]),
        [x, y, _, z] => Ok([x, y, z]),
        _ => Err(ProgramError::InvalidArgument),
    }?;

    check_valid_funding_account(funding_account)?;
    check_valid_writable_account(program_id, price_account)?;
    // Check clock
    let clock = Clock::from_account_info(clock_account)?;

    let mut publisher_index: usize = 0;
    let latest_aggregate_price: PriceInfo;
    {
        // Verify that symbol account is initialized
        let price_data = load_checked::<PriceAccount>(price_account, cmd_args.header.version)?;

        // Verify that publisher is authorized
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


        latest_aggregate_price = price_data.agg_;
        let latest_publisher_price = price_data.comp_[publisher_index].latest_;

        // Check that publisher is publishing a more recent price
        pyth_assert(
            !is_component_update(cmd_args)?
                || cmd_args.publishing_slot > latest_publisher_price.pub_slot_,
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

    let account_len = price_account.try_data_len()?;
    if aggregate_updated && account_len == PRICE_ACCOUNT_SIZE {
        let mut price_account = load_account_as_mut::<PriceAccountWrapper>(price_account)?;
        price_account.add_price_to_time_machine()?;
    }

    // Try to update the publisher's price
    if is_component_update(cmd_args)? {
        let mut status: u32 = cmd_args.status;
        let mut threshold_conf = cmd_args.price / PC_MAX_CI_DIVISOR as i64;

        if threshold_conf < 0 {
            threshold_conf = -threshold_conf;
        }

        if cmd_args.confidence > try_convert::<_, u64>(threshold_conf)? {
            status = PC_STATUS_UNKNOWN
        }

        {
            let mut price_data =
                load_checked::<PriceAccount>(price_account, cmd_args.header.version)?;
            let publisher_price = &mut price_data.comp_[publisher_index].latest_;
            publisher_price.price_ = cmd_args.price;
            publisher_price.conf_ = cmd_args.confidence;
            publisher_price.status_ = status;
            publisher_price.pub_slot_ = cmd_args.publishing_slot;
        }
    }

    Ok(())
}

pub fn upd_price_no_fail_on_error(
    program_id: &Pubkey,
    accounts: &[AccountInfo],
    instruction_data: &[u8],
) -> ProgramResult {
    match upd_price(program_id, accounts, instruction_data) {
        Err(_) => Ok(()),
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
) -> ProgramResult {
    let cmd_args = load::<AddPriceArgs>(instruction_data)?;

    check_exponent_range(cmd_args.exponent)?;
    pyth_assert(
        cmd_args.price_type != PC_PTYPE_UNKNOWN,
        ProgramError::InvalidArgument,
    )?;


    let [funding_account, product_account, price_account] = match accounts {
        [x, y, z] => Ok([x, y, z]),
        _ => Err(ProgramError::InvalidArgument),
    }?;

    check_valid_funding_account(funding_account)?;
    check_valid_signable_account(program_id, product_account)?;
    check_valid_signable_account(program_id, price_account)?;

    let mut product_data =
        load_checked::<ProductAccount>(product_account, cmd_args.header.version)?;

    let mut price_data =
        initialize_pyth_account_checked::<PriceAccount>(price_account, cmd_args.header.version)?;
    price_data.exponent = cmd_args.exponent;
    price_data.price_type = cmd_args.price_type;
    pubkey_assign(
        &mut price_data.product_account,
        &product_account.key.to_bytes(),
    );
    pubkey_assign(
        &mut price_data.next_price_account,
        bytes_of(&product_data.first_price_account),
    );
    pubkey_assign(
        &mut product_data.first_price_account,
        &price_account.key.to_bytes(),
    );

    Ok(())
}

/// Delete a price account. This function will remove the link between the price account and its
/// corresponding product account, then transfer any SOL in the price account to the funding
/// account. This function can only delete the first price account in the linked list of
/// price accounts for the given product.
///
/// Warning: This function is dangerous and will break any programs that depend on the deleted
/// price account!
pub fn del_price(
    program_id: &Pubkey,
    accounts: &[AccountInfo],
    instruction_data: &[u8],
) -> ProgramResult {
    let [funding_account, product_account, price_account] = match accounts {
        [w, x, y] => Ok([w, x, y]),
        _ => Err(ProgramError::InvalidArgument),
    }?;

    check_valid_funding_account(funding_account)?;
    check_valid_signable_account(program_id, product_account)?;
    check_valid_signable_account(program_id, price_account)?;

    {
        let cmd_args = load::<CommandHeader>(instruction_data)?;
        let mut product_data = load_checked::<ProductAccount>(product_account, cmd_args.version)?;
        let price_data = load_checked::<PriceAccount>(price_account, cmd_args.version)?;
        pyth_assert(
            pubkey_equal(
                &product_data.first_price_account,
                &price_account.key.to_bytes(),
            ),
            ProgramError::InvalidArgument,
        )?;

        pyth_assert(
            pubkey_equal(&price_data.product_account, &product_account.key.to_bytes()),
            ProgramError::InvalidArgument,
        )?;

        pubkey_assign(
            &mut product_data.first_price_account,
            bytes_of(&price_data.next_price_account),
        );
    }

    // Zero out the balance of the price account to delete it.
    // Note that you can't use the system program's transfer instruction to do this operation, as
    // that instruction fails if the source account has any data.
    let lamports = price_account.lamports();
    **price_account.lamports.borrow_mut() = 0;
    **funding_account.lamports.borrow_mut() += lamports;

    Ok(())
}

pub fn init_price(
    program_id: &Pubkey,
    accounts: &[AccountInfo],
    instruction_data: &[u8],
) -> ProgramResult {
    let cmd_args = load::<InitPriceArgs>(instruction_data)?;

    check_exponent_range(cmd_args.exponent)?;

    let [funding_account, price_account] = match accounts {
        [x, y] => Ok([x, y]),
        _ => Err(ProgramError::InvalidArgument),
    }?;

    check_valid_funding_account(funding_account)?;
    check_valid_signable_account(program_id, price_account)?;

    let mut price_data = load_checked::<PriceAccount>(price_account, cmd_args.header.version)?;
    pyth_assert(
        price_data.price_type == cmd_args.price_type,
        ProgramError::InvalidArgument,
    )?;

    price_data.exponent = cmd_args.exponent;

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
        size_of::<PriceEma>(),
    );
    sol_memset(
        bytes_of_mut(&mut price_data.twac_),
        0,
        size_of::<PriceEma>(),
    );
    sol_memset(
        bytes_of_mut(&mut price_data.agg_),
        0,
        size_of::<PriceInfo>(),
    );
    for i in 0..(price_data.comp_.len() as usize) {
        sol_memset(
            bytes_of_mut(&mut price_data.comp_[i].agg_),
            0,
            size_of::<PriceInfo>(),
        );
        sol_memset(
            bytes_of_mut(&mut price_data.comp_[i].latest_),
            0,
            size_of::<PriceInfo>(),
        );
    }

    Ok(())
}

/// add a publisher to a price account
/// accounts[0] funding account                                   [signer writable]
/// accounts[1] price account to add the publisher to             [signer writable]
pub fn add_publisher(
    program_id: &Pubkey,
    accounts: &[AccountInfo],
    instruction_data: &[u8],
) -> ProgramResult {
    let cmd_args = load::<AddPublisherArgs>(instruction_data)?;

    pyth_assert(
        instruction_data.len() == size_of::<AddPublisherArgs>()
            && !pubkey_is_zero(&cmd_args.publisher),
        ProgramError::InvalidArgument,
    )?;

    let [funding_account, price_account] = match accounts {
        [x, y] => Ok([x, y]),
        _ => Err(ProgramError::InvalidArgument),
    }?;

    check_valid_funding_account(funding_account)?;
    check_valid_signable_account(program_id, price_account)?;

    let mut price_data = load_checked::<PriceAccount>(price_account, cmd_args.header.version)?;

    if price_data.num_ >= PC_COMP_SIZE {
        return Err(ProgramError::InvalidArgument);
    }

    for i in 0..(price_data.num_ as usize) {
        if pubkey_equal(&cmd_args.publisher, bytes_of(&price_data.comp_[i].pub_)) {
            return Err(ProgramError::InvalidArgument);
        }
    }

    let current_index: usize = try_convert(price_data.num_)?;
    sol_memset(
        bytes_of_mut(&mut price_data.comp_[current_index]),
        0,
        size_of::<PriceComponent>(),
    );
    pubkey_assign(
        &mut price_data.comp_[current_index].pub_,
        bytes_of(&cmd_args.publisher),
    );
    price_data.num_ += 1;
    price_data.header.size =
        try_convert::<_, u32>(size_of::<PriceAccount>() - size_of_val(&price_data.comp_))?
            + price_data.num_ * try_convert::<_, u32>(size_of::<PriceComponent>())?;
    Ok(())
}

/// add a publisher to a price account
/// accounts[0] funding account                                   [signer writable]
/// accounts[1] price account to delete the publisher from        [signer writable]
pub fn del_publisher(
    program_id: &Pubkey,
    accounts: &[AccountInfo],
    instruction_data: &[u8],
) -> ProgramResult {
    let cmd_args = load::<DelPublisherArgs>(instruction_data)?;

    pyth_assert(
        instruction_data.len() == size_of::<DelPublisherArgs>()
            && !pubkey_is_zero(&cmd_args.publisher),
        ProgramError::InvalidArgument,
    )?;

    let [funding_account, price_account] = match accounts {
        [x, y] => Ok([x, y]),
        _ => Err(ProgramError::InvalidArgument),
    }?;

    check_valid_funding_account(funding_account)?;
    check_valid_signable_account(program_id, price_account)?;

    let mut price_data = load_checked::<PriceAccount>(price_account, cmd_args.header.version)?;

    for i in 0..(price_data.num_ as usize) {
        if pubkey_equal(&cmd_args.publisher, bytes_of(&price_data.comp_[i].pub_)) {
            for j in i + 1..(price_data.num_ as usize) {
                price_data.comp_[j - 1] = price_data.comp_[j];
            }
            price_data.num_ -= 1;
            let current_index: usize = try_convert(price_data.num_)?;
            sol_memset(
                bytes_of_mut(&mut price_data.comp_[current_index]),
                0,
                size_of::<PriceComponent>(),
            );
            price_data.header.size =
                try_convert::<_, u32>(size_of::<PriceAccount>() - size_of_val(&price_data.comp_))?
                    + price_data.num_ * try_convert::<_, u32>(size_of::<PriceComponent>())?;
            return Ok(());
        }
    }
    Err(ProgramError::InvalidArgument)
}

pub fn add_product(
    program_id: &Pubkey,
    accounts: &[AccountInfo],
    instruction_data: &[u8],
) -> ProgramResult {
    let [funding_account, tail_mapping_account, new_product_account] = match accounts {
        [x, y, z] => Ok([x, y, z]),
        _ => Err(ProgramError::InvalidArgument),
    }?;

    check_valid_funding_account(funding_account)?;
    check_valid_signable_account(program_id, tail_mapping_account)?;
    check_valid_signable_account(program_id, new_product_account)?;

    let hdr = load::<CommandHeader>(instruction_data)?;
    let mut mapping_data = load_checked::<MappingAccount>(tail_mapping_account, hdr.version)?;
    // The mapping account must have free space to add the product account
    pyth_assert(
        mapping_data.number_of_products < PC_MAP_TABLE_SIZE,
        ProgramError::InvalidArgument,
    )?;

    initialize_pyth_account_checked::<ProductAccount>(new_product_account, hdr.version)?;

    let current_index: usize = try_convert(mapping_data.number_of_products)?;
    pubkey_assign(
        &mut mapping_data.products_list[current_index],
        bytes_of(&new_product_account.key.to_bytes()),
    );
    mapping_data.number_of_products += 1;
    mapping_data.header.size = try_convert::<_, u32>(
        size_of::<MappingAccount>() - size_of_val(&mapping_data.products_list),
    )? + mapping_data.number_of_products
        * try_convert::<_, u32>(size_of::<CPubkey>())?;

    Ok(())
}

/// Update the metadata associated with a product, overwriting any existing metadata.
/// The metadata is provided as a list of key-value pairs at the end of the `instruction_data`.
pub fn upd_product(
    program_id: &Pubkey,
    accounts: &[AccountInfo],
    instruction_data: &[u8],
) -> ProgramResult {
    let [funding_account, product_account] = match accounts {
        [x, y] => Ok([x, y]),
        _ => Err(ProgramError::InvalidArgument),
    }?;

    check_valid_funding_account(funding_account)?;
    check_valid_signable_account(program_id, product_account)?;

    let hdr = load::<CommandHeader>(instruction_data)?;
    {
        // Validate that product_account contains the appropriate account header
        let mut _product_data = load_checked::<ProductAccount>(product_account, hdr.version)?;
    }

    pyth_assert(
        instruction_data.len() >= size_of::<CommandHeader>(),
        ProgramError::InvalidInstructionData,
    )?;
    let new_data_len = instruction_data.len() - size_of::<CommandHeader>();
    let max_data_len = try_convert::<_, usize>(PC_PROD_ACC_SIZE)? - size_of::<ProductAccount>();
    pyth_assert(new_data_len <= max_data_len, ProgramError::InvalidArgument)?;

    let new_data = &instruction_data[size_of::<CommandHeader>()..instruction_data.len()];
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
            &mut data[size_of::<ProductAccount>()..],
            new_data,
            new_data.len(),
        );
    }

    let mut product_data = load_checked::<ProductAccount>(product_account, hdr.version)?;
    product_data.header.size = try_convert(size_of::<ProductAccount>() + new_data.len())?;

    Ok(())
}

pub fn set_min_pub(
    program_id: &Pubkey,
    accounts: &[AccountInfo],
    instruction_data: &[u8],
) -> ProgramResult {
    let cmd = load::<SetMinPubArgs>(instruction_data)?;

    pyth_assert(
        instruction_data.len() == size_of::<SetMinPubArgs>(),
        ProgramError::InvalidArgument,
    )?;

    let [funding_account, price_account] = match accounts {
        [x, y] => Ok([x, y]),
        _ => Err(ProgramError::InvalidArgument),
    }?;

    check_valid_funding_account(funding_account)?;
    check_valid_signable_account(program_id, price_account)?;

    let mut price_account_data = load_checked::<PriceAccount>(price_account, cmd.header.version)?;
    price_account_data.min_pub_ = cmd.minimum_publishers;

    Ok(())
}

/// Delete a product account and remove it from the product list of its associated mapping account.
/// The deleted product account must not have any price accounts.
///
/// This function allows you to delete products from non-tail mapping accounts. This ability is a
/// little weird, as it allows you to construct a list of multiple mapping accounts where non-tail
/// accounts have empty space. This is fine however; users should simply add new products to the
/// first available spot.
pub fn del_product(
    program_id: &Pubkey,
    accounts: &[AccountInfo],
    instruction_data: &[u8],
) -> ProgramResult {
    let [funding_account, mapping_account, product_account] = match accounts {
        [w, x, y] => Ok([w, x, y]),
        _ => Err(ProgramError::InvalidArgument),
    }?;

    check_valid_funding_account(funding_account)?;
    check_valid_signable_account(program_id, mapping_account)?;
    check_valid_signable_account(program_id, product_account)?;

    {
        let cmd_args = load::<CommandHeader>(instruction_data)?;
        let mut mapping_data = load_checked::<MappingAccount>(mapping_account, cmd_args.version)?;
        let product_data = load_checked::<ProductAccount>(product_account, cmd_args.version)?;

        // This assertion is just to make the subtractions below simpler
        pyth_assert(
            mapping_data.number_of_products >= 1,
            ProgramError::InvalidArgument,
        )?;
        pyth_assert(
            pubkey_is_zero(&product_data.first_price_account),
            ProgramError::InvalidArgument,
        )?;

        let product_key = product_account.key.to_bytes();
        let product_index = mapping_data
            .products_list
            .iter()
            .position(|x| pubkey_equal(x, &product_key))
            .ok_or(ProgramError::InvalidArgument)?;

        let num_after_removal: usize = try_convert(
            mapping_data
                .number_of_products
                .checked_sub(1)
                .ok_or(ProgramError::InvalidArgument)?,
        )?;

        let last_key_bytes = mapping_data.products_list[num_after_removal];
        pubkey_assign(
            &mut mapping_data.products_list[product_index],
            bytes_of(&last_key_bytes),
        );
        pubkey_clear(&mut mapping_data.products_list[num_after_removal]);
        mapping_data.number_of_products = try_convert::<_, u32>(num_after_removal)?;
        mapping_data.header.size = try_convert::<_, u32>(
            size_of::<MappingAccount>() - size_of_val(&mapping_data.products_list),
        )? + mapping_data.number_of_products
            * try_convert::<_, u32>(size_of::<CPubkey>())?;
    }

    // Zero out the balance of the price account to delete it.
    // Note that you can't use the system program's transfer instruction to do this operation, as
    // that instruction fails if the source account has any data.
    let lamports = product_account.lamports();
    **product_account.lamports.borrow_mut() = 0;
    **funding_account.lamports.borrow_mut() += lamports;

    Ok(())
}
