use std::mem::size_of;

use bytemuck::{
    try_from_bytes,
    try_from_bytes_mut,
    Pod,
};

use crate::c_oracle_header::{
    pc_acc,
    PythAccount,
    PC_MAGIC,
};
use crate::error::OracleError;
use crate::utils::{
    clear_account,
    pyth_assert,
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

pub fn initialize_pyth_account_checked<'a, T: PythAccount>(
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
