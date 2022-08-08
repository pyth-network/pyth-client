use std::mem::size_of;

use bytemuck::{
    try_from_bytes,
    try_from_bytes_mut,
    Pod,
};

use std::cell::{
    Ref,
    RefMut,
};

use solana_program::account_info::AccountInfo;
use solana_program::program_error::ProgramError;

/// Interpret the bytes in `data` as a value of type `T`
pub fn load<T: Pod>(data: &[u8]) -> Result<&T, ProgramError> {
    try_from_bytes(&data[0..size_of::<T>()]).map_err(|_| ProgramError::InvalidArgument)
}

/// Interpret the bytes in `data` as a mutable value of type `T`
#[allow(unused)]
pub fn load_mut<T: Pod>(data: &mut [u8]) -> Result<&mut T, ProgramError> {
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
