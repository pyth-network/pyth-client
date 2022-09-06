use std::mem::size_of;

use bytemuck::{
    try_from_bytes,
    try_from_bytes_mut,
    Pod,
};
use solana_program::pubkey::Pubkey;
use solana_program::rent::Rent;

use crate::c_oracle_header::{
    AccountHeader,
    PythAccount,
    PC_MAGIC,
};
use crate::error::OracleError;
use crate::utils::{
    allocate_data,
    assign_owner,
    check_valid_fresh_account,
    clear_account,
    pyth_assert,
    send_lamports,
};
use solana_program::account_info::AccountInfo;
use solana_program::program_error::ProgramError;
use std::cell::{
    Ref,
    RefMut,
};

/// Interpret the bytes in `data` as a value of type `T`
/// This will fail if :
/// - `data` is too short
/// - `data` is not aligned for T
pub fn load<T: Pod>(data: &[u8]) -> Result<&T, OracleError> {
    try_from_bytes(
        data.get(0..size_of::<T>())
            .ok_or(OracleError::InstructionDataTooShort)?,
    )
    .map_err(|_| OracleError::InstructionDataSliceMisaligned)
}

/// Interpret the bytes in `data` as a mutable value of type `T`
#[allow(unused)]
pub fn load_mut<T: Pod>(data: &mut [u8]) -> Result<&mut T, OracleError> {
    try_from_bytes_mut(
        data.get_mut(0..size_of::<T>())
            .ok_or(OracleError::InstructionDataTooShort)?,
    )
    .map_err(|_| OracleError::InstructionDataSliceMisaligned)
}

/// Get the data stored in `account` as a value of type `T`.
/// WARNING : Use `load_checked` to load initialized Pyth accounts
pub fn load_account_as<'a, T: Pod>(account: &'a AccountInfo) -> Result<Ref<'a, T>, ProgramError> {
    let data = account.try_borrow_data()?;

    Ok(Ref::map(data, |data| {
        bytemuck::from_bytes(&data[0..size_of::<T>()])
    }))
}

/// Mutably borrow the data in `account` as a value of type `T`.
/// Any mutations to the returned value will be reflected in the account data.
/// WARNING : Use `load_checked` to load initialized Pyth accounts
pub fn load_account_as_mut<'a, T: Pod>(
    account: &'a AccountInfo,
) -> Result<RefMut<'a, T>, ProgramError> {
    let data = account.try_borrow_mut_data()?;

    Ok(RefMut::map(data, |data| {
        bytemuck::from_bytes_mut(&mut data[0..size_of::<T>()])
    }))
}

pub fn load_checked<'a, T: PythAccount>(
    account: &'a AccountInfo,
    version: u32,
) -> Result<RefMut<'a, T>, ProgramError> {
    pyth_assert(
        account.data_len() >= T::MINIMUM_SIZE,
        OracleError::AccountTooSmall.into(),
    )?;

    {
        let account_header = load_account_as::<AccountHeader>(account)?;
        pyth_assert(
            account_header.magic_number == PC_MAGIC
                && account_header.version == version
                && account_header.account_type == T::ACCOUNT_TYPE,
            ProgramError::InvalidArgument,
        )?;
    }

    load_account_as_mut::<T>(account)
}

pub fn initialize_pyth_account_checked<'a, T: PythAccount>(
    account: &'a AccountInfo,
    version: u32,
) -> Result<RefMut<'a, T>, ProgramError> {
    pyth_assert(
        account.data_len() >= T::MINIMUM_SIZE,
        OracleError::AccountTooSmall.into(),
    )?;
    check_valid_fresh_account(account)?;
    clear_account(account)?;

    {
        let mut account_header = load_account_as_mut::<AccountHeader>(account)?;
        account_header.magic_number = PC_MAGIC;
        account_header.version = version;
        account_header.account_type = T::ACCOUNT_TYPE;
        account_header.size = T::INITIAL_SIZE;
    }

    load_account_as_mut::<T>(account)
}

// Creates pda if needed and initializes it as one of the Pyth accounts
pub fn create_pda_if_needed<'a, T: PythAccount>(
    account: &AccountInfo<'a>,
    funding_account: &AccountInfo<'a>,
    system_program: &AccountInfo<'a>,
    program_id: &Pubkey,
    seeds: &[&[u8]],
    version: u32,
) -> Result<(), ProgramError> {
    let target_rent = Rent::default().minimum_balance(T::MINIMUM_SIZE);
    if account.lamports() < target_rent {
        send_lamports(
            funding_account,
            account,
            system_program,
            target_rent - account.lamports(),
        )?;
    }
    if account.data_len() == 0 {
        allocate_data(account, system_program, T::MINIMUM_SIZE, seeds)?;
        assign_owner(account, program_id, system_program, seeds)?;
        initialize_pyth_account_checked::<T>(account, version)?;
    }
    Ok(())
}
