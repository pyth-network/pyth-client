use std::borrow::BorrowMut;
use std::cell::RefMut;
use std::mem::{
    size_of,
    size_of_val,
};

use crate::deserialize::{
    load,
    load_account_as,
    load_account_as_mut,
};

use bytemuck::bytes_of;

use solana_program::account_info::AccountInfo;
use solana_program::entrypoint::SUCCESS;
use solana_program::msg;
use solana_program::program_error::ProgramError;
use solana_program::program_memory::sol_memset;
use solana_program::pubkey::Pubkey;
use solana_program::rent::Rent;


use crate::time_machine_types::PriceAccountWrapper;
use solana_program::program::invoke;
use solana_program::system_instruction::transfer;
use solana_program::system_program::check_id;


use crate::c_oracle_header::{
    cmd_add_price_t,
    cmd_hdr_t,
    pc_acc,
    pc_map_table_t,
    pc_price_t,
    pc_prod_t,
    pc_pub_key_t,
    PC_ACCTYPE_MAPPING,
    PC_ACCTYPE_PRICE,
    PC_ACCTYPE_PRODUCT,
    PC_MAGIC,
    PC_MAP_TABLE_SIZE,
    PC_MAX_NUM_DECIMALS,
    PC_PROD_ACC_SIZE,
    PC_PTYPE_UNKNOWN,
    SUCCESSFULLY_UPDATED_AGGREGATE,
};
use crate::error::OracleResult;
use crate::OracleError;

use crate::utils::pyth_assert;

use super::c_entrypoint_wrapper;

const PRICE_T_SIZE: usize = size_of::<pc_price_t>();
const PRICE_ACCOUNT_SIZE: usize = size_of::<PriceAccountWrapper>();


///Calls the c oracle update_price, and updates the Time Machine if needed
pub fn update_price(
    _program_id: &Pubkey,
    accounts: &[AccountInfo],
    _instruction_data: &[u8],
    input: *mut u8,
) -> OracleResult {
    let price_account_info = &accounts[1];
    let account_len = price_account_info.try_data_len()?;
    if account_len < PRICE_ACCOUNT_SIZE {
        if account_len != PRICE_T_SIZE {
            return Err(ProgramError::InvalidArgument);
        }
        msg!("Please resize the account to allow for SMA tracking!");
        return c_entrypoint_wrapper(input);
    }
    let c_ret_value = c_entrypoint_wrapper(input)?;
    if c_ret_value == SUCCESSFULLY_UPDATED_AGGREGATE {
        let mut price_account = load_account_as_mut::<PriceAccountWrapper>(price_account_info)?;
        price_account.add_price()?;
        msg!("updated tracker!");
    }
    Ok(c_ret_value)
}
fn send_lamports<'a>(
    from: &AccountInfo<'a>,
    to: &AccountInfo<'a>,
    system_program: &AccountInfo<'a>,
    ammount: u64,
) -> Result<(), ProgramError> {
    let transfer_instruction = transfer(from.key, to.key, ammount);
    invoke(
        &transfer_instruction,
        &[from.clone(), to.clone(), system_program.clone()],
    )?;
    Ok(())
}

///resize price account and initialize the TimeMachineStructure
pub fn upgrade_price_account<'a>(
    funding_account_info: &AccountInfo<'a>,
    price_account_info: &AccountInfo<'a>,
    system_program: &AccountInfo<'a>,
    program_id: &Pubkey,
) -> OracleResult {
    check_valid_funding_account(funding_account_info)?;
    check_valid_signable_account(program_id, price_account_info, size_of::<pc_price_t>())?;
    let account_len = price_account_info.try_data_len()?;
    match account_len {
        PRICE_T_SIZE => {
            //compute the number of lamports needed in the price account to update it
            let rent: Rent = Default::default();
            let lamports_needed: u64 = rent
                .minimum_balance(size_of::<PriceAccountWrapper>())
                .saturating_sub(price_account_info.lamports());
            //transfer lamports if necessary
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
            //update twap_tracker
            let mut price_account = load_account_as_mut::<PriceAccountWrapper>(price_account_info)?;
            price_account.add_first_price()?;
            Ok(SUCCESS)
        }
        PRICE_ACCOUNT_SIZE => Ok(SUCCESS),
        _ => Err(ProgramError::InvalidArgument),
    }
}

fn get_account_type(account: &AccountInfo) -> Result<u32, ProgramError> {
    let account_type = load_account_as::<pc_acc>(account)?.type_;
    Ok(account_type)
}

/// has version number/ account type dependant logic to make sure the given account is compatible
/// with the current version
/// updates the version number for all accounts, and resizes price accounts
/// accounts[0] funding account           [signer writable]
/// accounts[1] upgradded acount       [signer writable]
/// accounts [2] system program
pub fn update_version(
    program_id: &Pubkey,
    accounts: &[AccountInfo],
    _instruction_data: &[u8],
) -> OracleResult {
    let funding_account_info = &accounts[0];
    let upgraded_account_info = &accounts[1];
    let system_program = &accounts[2];

    pyth_assert(
        check_id(system_program.key),
        OracleError::InvalidSystemAccount.into(),
    )?;

    //read account info
    let pyth_acc_type = get_account_type(upgraded_account_info)?;

    //Note: currently we do not seem to need to do anything with the version number,
    // but such logic can be added here, or in the per account type upgrades below

    match pyth_acc_type {
        PC_ACCTYPE_PRICE => upgrade_price_account(
            funding_account_info,
            upgraded_account_info,
            system_program,
            program_id,
        ),
        _ => Ok(SUCCESS),
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
    initialize_mapping_account(fresh_mapping_account, hdr.ver_)?;

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
    let mut cur_mapping = load_mapping_account_mut(cur_mapping, hdr.ver_)?;
    pyth_assert(
        cur_mapping.num_ == PC_MAP_TABLE_SIZE
            && unsafe { cur_mapping.next_.k8_.iter().all(|x| *x == 0) },
        ProgramError::InvalidArgument,
    )?;

    initialize_mapping_account(next_mapping, hdr.ver_)?;
    pubkey_assign(&mut cur_mapping.next_, &next_mapping.key.to_bytes());

    Ok(SUCCESS)
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

    if cmd_args.expo_ > PC_MAX_NUM_DECIMALS as i32
        || cmd_args.expo_ < -(PC_MAX_NUM_DECIMALS as i32)
        || cmd_args.ptype_ == PC_PTYPE_UNKNOWN
    {
        return Err(ProgramError::InvalidArgument);
    }


    let [funding_account, product_account, price_account] = match accounts {
        [x, y, z] => Ok([x, y, z]),
        _ => Err(ProgramError::InvalidArgument),
    }?;

    check_valid_funding_account(funding_account)?;
    check_valid_signable_account(program_id, product_account, PC_PROD_ACC_SIZE as usize)?;
    check_valid_signable_account(program_id, price_account, size_of::<pc_price_t>())?;
    check_valid_fresh_account(price_account)?;

    let mut product_data = load_product_account_mut(product_account, cmd_args.ver_)?;

    clear_account(price_account)?;

    let mut price_data = load_account_as_mut::<pc_price_t>(price_account)?;
    price_data.magic_ = PC_MAGIC;
    price_data.ver_ = cmd_args.ver_;
    price_data.type_ = PC_ACCTYPE_PRICE;
    price_data.size_ = (size_of::<pc_price_t>() - size_of_val(&price_data.comp_)) as u32;
    price_data.expo_ = cmd_args.expo_;
    price_data.ptype_ = cmd_args.ptype_;
    pubkey_assign(&mut price_data.prod_, &product_account.key.to_bytes());
    pubkey_assign(&mut price_data.next_, bytes_of(&product_data.px_acc_));
    pubkey_assign(&mut product_data.px_acc_, &price_account.key.to_bytes());

    Ok(SUCCESS)
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
    let mut mapping_data = load_mapping_account_mut(tail_mapping_account, hdr.ver_)?;
    // The mapping account must have free space to add the product account
    pyth_assert(
        mapping_data.num_ < PC_MAP_TABLE_SIZE,
        ProgramError::InvalidArgument,
    )?;

    initialize_product_account(new_product_account, hdr.ver_)?;

    let current_index: usize = try_convert(mapping_data.num_)?;
    unsafe {
        mapping_data.prod_[current_index]
            .k1_
            .copy_from_slice(&new_product_account.key.to_bytes())
    }
    mapping_data.num_ += 1;
    mapping_data.size_ =
        try_convert::<_, u32>(size_of::<pc_map_table_t>() - size_of_val(&mapping_data.prod_))?
            + mapping_data.num_ * try_convert::<_, u32>(size_of::<pc_pub_key_t>())?;

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
    pyth_assert(valid_fresh_account(account), ProgramError::InvalidArgument)
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


/// Mutably borrow the data in `account` as a mapping account, validating that the account
/// is properly formatted. Any mutations to the returned value will be reflected in the
/// account data. Use this to read already-initialized accounts.
pub fn load_mapping_account_mut<'a>(
    account: &'a AccountInfo,
    expected_version: u32,
) -> Result<RefMut<'a, pc_map_table_t>, ProgramError> {
    let mapping_data = load_account_as_mut::<pc_map_table_t>(account)?;

    pyth_assert(
        mapping_data.magic_ == PC_MAGIC
            && mapping_data.ver_ == expected_version
            && mapping_data.type_ == PC_ACCTYPE_MAPPING,
        ProgramError::InvalidArgument,
    )?;

    Ok(mapping_data)
}

/// Initialize account as a new mapping account. This function will zero out any existing data in
/// the account.
pub fn initialize_mapping_account(account: &AccountInfo, version: u32) -> Result<(), ProgramError> {
    clear_account(account)?;

    let mut mapping_account = load_account_as_mut::<pc_map_table_t>(account)?;
    mapping_account.magic_ = PC_MAGIC;
    mapping_account.ver_ = version;
    mapping_account.type_ = PC_ACCTYPE_MAPPING;
    mapping_account.size_ =
        try_convert(size_of::<pc_map_table_t>() - size_of_val(&mapping_account.prod_))?;

    Ok(())
}

/// Initialize account as a new product account. This function will zero out any existing data in
/// the account.
pub fn initialize_product_account(account: &AccountInfo, version: u32) -> Result<(), ProgramError> {
    clear_account(account)?;

    let mut prod_account = load_account_as_mut::<pc_prod_t>(account)?;
    prod_account.magic_ = PC_MAGIC;
    prod_account.ver_ = version;
    prod_account.type_ = PC_ACCTYPE_PRODUCT;
    prod_account.size_ = try_convert(size_of::<pc_prod_t>())?;

    Ok(())
}

/// Mutably borrow the data in `account` as a product account, validating that the account
/// is properly formatted. Any mutations to the returned value will be reflected in the
/// account data. Use this to read already-initialized accounts.
pub fn load_product_account_mut<'a>(
    account: &'a AccountInfo,
    expected_version: u32,
) -> Result<RefMut<'a, pc_prod_t>, ProgramError> {
    let product_data = load_account_as_mut::<pc_prod_t>(account)?;

    pyth_assert(
        product_data.magic_ == PC_MAGIC
            && product_data.ver_ == expected_version
            && product_data.type_ == PC_ACCTYPE_PRODUCT,
        ProgramError::InvalidArgument,
    )?;

    Ok(product_data)
}

// Assign pubkey bytes from source to target, fails if source is not 32 bytes
pub fn pubkey_assign(target: &mut pc_pub_key_t, source: &[u8]) {
    unsafe { target.k1_.copy_from_slice(source) }
}

/// Convert `x: T` into a `U`, returning the appropriate `OracleError` if the conversion fails.
fn try_convert<T, U: TryFrom<T>>(x: T) -> Result<U, OracleError> {
    // Note: the error here assumes we're only applying this function to integers right now.
    U::try_from(x).map_err(|_| OracleError::IntegerCastingError)
}
