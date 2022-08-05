use std::borrow::BorrowMut;
use std::cell::{
    Ref,
    RefMut,
};
use std::mem::{
    size_of,
    size_of_val,
};

use bytemuck::{
    bytes_of,
    try_from_bytes,
    try_from_bytes_mut,
    Pod,
};
use solana_program::account_info::AccountInfo;
use solana_program::entrypoint::SUCCESS;
use solana_program::program_error::ProgramError;
use solana_program::program_memory::sol_memset;
use solana_program::pubkey::Pubkey;
use solana_program::rent::Rent;

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
};
use crate::error::OracleResult;

use crate::utils::pyth_assert;

use super::c_entrypoint_wrapper;

///Calls the c oracle update_price, and updates the Time Machine if needed
pub fn update_price(
    _program_id: &Pubkey,
    _accounts: &[AccountInfo],
    _instruction_data: &[u8],
    input: *mut u8,
) -> OracleResult {
    //For now, we did not change the behavior of this. this is just to show the proposed structure
    // of the program
    c_entrypoint_wrapper(input)
}
/// has version number/ account type dependant logic to make sure the given account is compatible
/// with the current version
/// updates the version number for all accounts, and resizes price accounts
pub fn update_version(
    _program_id: &Pubkey,
    _accounts: &[AccountInfo],
    _instruction_data: &[u8],
) -> OracleResult {
    panic!("Need to merge fix to pythd in order to implement this");
    // Ok(SUCCESS)
}

/// initialize the first mapping account in a new linked-list of mapping accounts
/// accounts[0] funding account           [signer writable]
/// accounts[1] new mapping account       [signer writable]
pub fn init_mapping(
    program_id: &Pubkey,
    accounts: &[AccountInfo],
    instruction_data: &[u8],
) -> OracleResult {
    let [_funding_account, fresh_mapping_account] = match accounts {
        [x, y]
            if valid_funding_account(x)
                && valid_signable_account(program_id, y, size_of::<pc_map_table_t>())
                && valid_fresh_account(y) =>
        {
            Ok([x, y])
        }
        _ => Err(ProgramError::InvalidArgument),
    }?;

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
    let [_funding_account, cur_mapping, next_mapping] = match accounts {
        [x, y, z]
            if valid_funding_account(x)
                && valid_signable_account(program_id, y, size_of::<pc_map_table_t>())
                && valid_signable_account(program_id, z, size_of::<pc_map_table_t>())
                && valid_fresh_account(z) =>
        {
            Ok([x, y, z])
        }
        _ => Err(ProgramError::InvalidArgument),
    }?;

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

    let [_funding_account, product_account, price_account] = match accounts {
        [x, y, z]
            if valid_funding_account(x)
                && valid_signable_account(program_id, y, PC_PROD_ACC_SIZE as usize)
                && valid_signable_account(program_id, z, size_of::<pc_price_t>())
                && valid_fresh_account(z) =>
        {
            Ok([x, y, z])
        }
        _ => Err(ProgramError::InvalidArgument),
    }?;

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

fn valid_funding_account(account: &AccountInfo) -> bool {
    account.is_signer && account.is_writable
}

fn valid_signable_account(program_id: &Pubkey, account: &AccountInfo, minimum_size: usize) -> bool {
    account.is_signer
        && account.is_writable
        && account.owner == program_id
        && account.data_len() >= minimum_size
        && Rent::default().is_exempt(account.lamports(), account.data_len())
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

/// Sets the data of account to all-zero
pub fn clear_account(account: &AccountInfo) -> Result<(), ProgramError> {
    let mut data = account
        .try_borrow_mut_data()
        .map_err(|_| ProgramError::InvalidArgument)?;
    let length = data.len();
    sol_memset(data.borrow_mut(), 0, length);
    Ok(())
}

/// Interpret the bytes in `data` as a value of type `T`
fn load<T: Pod>(data: &[u8]) -> Result<&T, ProgramError> {
    try_from_bytes(
        data.get(0..size_of::<T>())
            .ok_or(ProgramError::InvalidArgument)?,
    )
    .map_err(|_| ProgramError::InvalidArgument)
}

/// Interpret the bytes in `data` as a mutable value of type `T`
#[allow(unused)]
fn load_mut<T: Pod>(data: &mut [u8]) -> Result<&mut T, ProgramError> {
    try_from_bytes_mut(&mut data[0..size_of::<T>()]).map_err(|_| ProgramError::InvalidArgument)
}

/// Get the data stored in `account` as a value of type `T`
pub fn load_account_as<'a, T: Pod>(account: &'a AccountInfo) -> Result<Ref<'a, T>, ProgramError> {
    let data = account.try_borrow_data()?;

    Ok(Ref::map(data, |data| {
        bytemuck::from_bytes(&data[0..size_of::<T>()])
    }))
}

/// Mutably borrow the data in `account` as a value of type `T`.
/// Any mutations to the returned value will be reflected in the account data.
pub fn load_account_as_mut<'a, T: Pod>(
    account: &'a AccountInfo,
) -> Result<RefMut<'a, T>, ProgramError> {
    let data = account.try_borrow_mut_data()?;

    Ok(RefMut::map(data, |data| {
        bytemuck::from_bytes_mut(&mut data[0..size_of::<T>()])
    }))
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
        (size_of::<pc_map_table_t>() - size_of_val(&mapping_account.prod_)) as u32;

    Ok(())
}

/// Mutably borrow the data in `account` as a product account, validating that the account
/// is properly formatted. Any mutations to the returned value will be reflected in the
/// account data. Use this to read already-initialized accounts.
fn load_product_account_mut<'a>(
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
fn pubkey_assign(target: &mut pc_pub_key_t, source: &[u8]) {
    unsafe { target.k1_.copy_from_slice(source) }
}
