use crate::c_oracle_header::size_t;
use crate::error::OracleError;
use borsh::BorshDeserialize;
use solana_program::account_info::AccountInfo;
use solana_program::program_error::ProgramError;
use std::mem::size_of;
use std::result::Result;

/// Deserialize field in `source` with offset `offset`
pub fn deserialize_single_field_from_buffer<T: BorshDeserialize>(
    source: &[u8],
    offset: Option<size_t>,
) -> Result<T, ProgramError> {
    let start: usize = offset
        .unwrap_or(0)
        .try_into()
        .map_err(|_| OracleError::IntegerCastingError)?;

    let res: T = T::try_from_slice(&source[start..(start + size_of::<T>())])?;
    Ok(res)
}

/// Deserialize field in `i` rank of `accounts` with offset `offset`
pub fn deserialize_single_field_from_account<T: BorshDeserialize>(
    accounts: &[AccountInfo],
    i: usize,
    offset: Option<size_t>,
) -> Result<T, ProgramError> {
    Ok(deserialize_single_field_from_buffer::<T>(
        &accounts
            .get(i)
            .ok_or(ProgramError::NotEnoughAccountKeys)?
            .try_borrow_data()?,
        offset,
    )?)
}
