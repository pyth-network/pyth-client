use {
    crate::{
        accounts::{
            AccountHeader,
            PythAccount,
        },
        c_oracle_header::PC_MAGIC,
        error::OracleError,
        utils::pyth_assert,
    },
    bytemuck::{
        try_from_bytes,
        try_from_bytes_mut,
        Pod,
    },
    solana_program::{
        account_info::AccountInfo,
        program_error::ProgramError,
    },
    std::{
        cell::{
            Ref,
            RefMut,
        },
        mem::size_of,
    },
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
            OracleError::InvalidAccountHeader.into(),
        )?;
    }

    load_account_as_mut::<T>(account)
}
