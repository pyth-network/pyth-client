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
    from_bytes,
    from_bytes_mut,
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
    pc_map_table_t,
    PC_ACCTYPE_MAPPING_V,
    PC_MAGIC_V,
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
    pyth_assert(
        accounts.len() == 2
            && valid_funding_account(accounts.get(0).unwrap())
            && valid_signable_account(
                program_id,
                accounts.get(1).unwrap(),
                size_of::<pc_map_table_t>(),
            ),
        ProgramError::InvalidArgument,
    )?;

    // Check that the account has not already been initialized
    {
        let mapping_account = load_account_as::<pc_map_table_t>(accounts.get(1).unwrap())
            .map_err(|_| ProgramError::InvalidArgument)?;
        pyth_assert(
            mapping_account.magic_ == 0 && mapping_account.ver_ == 0,
            ProgramError::InvalidArgument,
        )?;
    }

    // Initialize by setting to zero again (just in case) and setting
    // the version number
    let hdr = load::<cmd_hdr_t>(instruction_data);
    {
        let mut data = accounts
            .get(1)
            .unwrap()
            .try_borrow_mut_data()
            .map_err(|_| ProgramError::InvalidArgument)?;
        sol_memset(data.borrow_mut(), 0, size_of::<pc_map_table_t>());
    }

    {
        let mut mapping_account = load_account_as_mut::<pc_map_table_t>(accounts.get(1).unwrap())
            .map_err(|_| ProgramError::InvalidArgument)?;
        mapping_account.magic_ = PC_MAGIC_V;
        mapping_account.ver_ = hdr.ver_;
        mapping_account.type_ = PC_ACCTYPE_MAPPING_V;
        mapping_account.size_ =
            (size_of::<pc_map_table_t>() - size_of_val(&mapping_account.prod_)) as u32;
    }

    Result::Ok(SUCCESS)
}

pub fn pyth_assert(condition: bool, error_code: ProgramError) -> Result<(), ProgramError> {
    if !condition {
        Result::Err(error_code)
    } else {
        Result::Ok(())
    }
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

/// Interpret the bytes in `data` as a value of type `T`
fn load<T: Pod>(data: &[u8]) -> &T {
    from_bytes(&data[0..size_of::<T>()])
}

/// Interpret the bytes in `data` as a mutable value of type `T`
#[allow(unused)]
fn load_mut<T: Pod>(data: &mut [u8]) -> &mut T {
    from_bytes_mut(&mut data[0..size_of::<T>()])
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
