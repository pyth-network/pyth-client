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
    try_from_bytes,
    try_from_bytes_mut,
    Pod,
};
use solana_program::entrypoint::SUCCESS;
use solana_program::program_error::ProgramError;
use solana_program::program_memory::sol_memset;
use solana_program::pubkey::Pubkey;
use solana_program::rent::Rent;
use solana_program::sysvar::slot_history::AccountInfo;

use crate::c_oracle_header::{
    cmd_hdr_t,
    pc_acc,
    pc_map_table_t,
    PC_ACCTYPE_MAPPING,
    PC_MAGIC,
};
use crate::error::OracleResult;

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
    clear_account(fresh_mapping_account)?;

    let hdr = load::<cmd_hdr_t>(instruction_data)?;
    let mut mapping_data = load_account_as_mut::<pc_map_table_t>(fresh_mapping_account)?;
    mapping_data.magic_ = PC_MAGIC;
    mapping_data.ver_ = hdr.ver_;
    mapping_data.type_ = PC_ACCTYPE_MAPPING;
    mapping_data.size_ = (size_of::<pc_map_table_t>() - size_of_val(&mapping_data.prod_)) as u32;

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
fn clear_account(account: &AccountInfo) -> Result<(), ProgramError> {
    let mut data = account
        .try_borrow_mut_data()
        .map_err(|_| ProgramError::InvalidArgument)?;
    let length = data.len();
    sol_memset(data.borrow_mut(), 0, length);
    Ok(())
}

/// Interpret the bytes in `data` as a value of type `T`
fn load<T: Pod>(data: &[u8]) -> Result<&T, ProgramError> {
    try_from_bytes(&data[0..size_of::<T>()]).map_err(|_| ProgramError::InvalidArgument)
}

/// Interpret the bytes in `data` as a mutable value of type `T`
#[allow(unused)]
fn load_mut<T: Pod>(data: &mut [u8]) -> Result<&mut T, ProgramError> {
    try_from_bytes_mut(&mut data[0..size_of::<T>()]).map_err(|_| ProgramError::InvalidArgument)
}

/// Get the data stored in `account` as a value of type `T`
fn load_account_as<'a, T: Pod>(account: &'a AccountInfo) -> Result<Ref<'a, T>, ProgramError> {
    let data = account.try_borrow_data()?;

    Ok(Ref::map(data, |data| {
        bytemuck::from_bytes(&data[0..size_of::<T>()])
    }))
}

/// Mutably borrow the data in `account` as a value of type `T`.
/// Any mutations to the returned value will be reflected in the account data.
fn load_account_as_mut<'a, T: Pod>(
    account: &'a AccountInfo,
) -> Result<RefMut<'a, T>, ProgramError> {
    let data = account.try_borrow_mut_data()?;

    Ok(RefMut::map(data, |data| {
        bytemuck::from_bytes_mut(&mut data[0..size_of::<T>()])
    }))
}
