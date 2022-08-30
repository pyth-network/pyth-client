use crate::c_oracle_header::{
    AccountHeader,
    CPubkey,
    PC_MAX_NUM_DECIMALS,
};
use crate::deserialize::load_account_as;
use crate::instruction::{
    OracleCommand,
    UpdPriceArgs,
};
use crate::OracleError;
use num_traits::FromPrimitive;
use solana_program::account_info::AccountInfo;
use solana_program::program_error::ProgramError;
use solana_program::program_memory::sol_memset;
use solana_program::pubkey::Pubkey;
use solana_program::sysvar::rent::Rent;
use std::borrow::BorrowMut;


pub fn pyth_assert(condition: bool, error_code: ProgramError) -> Result<(), ProgramError> {
    if !condition {
        Result::Err(error_code)
    } else {
        Result::Ok(())
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

pub fn valid_funding_account(account: &AccountInfo) -> bool {
    account.is_signer && account.is_writable
}

pub fn check_valid_funding_account(account: &AccountInfo) -> Result<(), ProgramError> {
    pyth_assert(
        valid_funding_account(account),
        OracleError::InvalidFundingAccount.into(),
    )
}

pub fn valid_signable_account(program_id: &Pubkey, account: &AccountInfo) -> bool {
    account.is_signer
        && account.is_writable
        && account.owner == program_id
        && Rent::default().is_exempt(account.lamports(), account.data_len())
}

pub fn check_valid_signable_account(
    program_id: &Pubkey,
    account: &AccountInfo,
) -> Result<(), ProgramError> {
    pyth_assert(
        valid_signable_account(program_id, account),
        OracleError::InvalidSignableAccount.into(),
    )
}

/// Returns `true` if the `account` is fresh, i.e., its data can be overwritten.
/// Use this check to prevent accidentally overwriting accounts whose data is already populated.
pub fn valid_fresh_account(account: &AccountInfo) -> bool {
    let pyth_acc = load_account_as::<AccountHeader>(account);
    match pyth_acc {
        Ok(pyth_acc) => pyth_acc.magic_number == 0 && pyth_acc.version == 0,
        Err(_) => false,
    }
}

pub fn check_valid_fresh_account(account: &AccountInfo) -> Result<(), ProgramError> {
    pyth_assert(
        valid_fresh_account(account),
        OracleError::InvalidFreshAccount.into(),
    )
}

// Check that an exponent is within the range of permitted exponents for price accounts.
pub fn check_exponent_range(expo: i32) -> Result<(), ProgramError> {
    pyth_assert(
        expo >= -(PC_MAX_NUM_DECIMALS as i32) && expo <= PC_MAX_NUM_DECIMALS as i32,
        ProgramError::InvalidArgument,
    )
}

// Assign pubkey bytes from source to target, fails if source is not 32 bytes
pub fn pubkey_assign(target: &mut CPubkey, source: &[u8]) {
    unsafe { target.k1_.copy_from_slice(source) }
}

pub fn pubkey_is_zero(key: &CPubkey) -> bool {
    return unsafe { key.k8_.iter().all(|x| *x == 0) };
}

pub fn pubkey_equal(target: &CPubkey, source: &[u8]) -> bool {
    unsafe { target.k1_ == *source }
}

/// Zero out the bytes of `key`.
pub fn pubkey_clear(key: &mut CPubkey) {
    unsafe {
        for i in 0..key.k8_.len() {
            key.k8_[i] = 0;
        }
    }
}

/// Convert `x: T` into a `U`, returning the appropriate `OracleError` if the conversion fails.
pub fn try_convert<T, U: TryFrom<T>>(x: T) -> Result<U, OracleError> {
    // Note: the error here assumes we're only applying this function to integers right now.
    U::try_from(x).map_err(|_| OracleError::IntegerCastingError)
}

/// Read a `pc_str_t` from the beginning of `source`. Returns a slice of `source` containing
/// the bytes of the `pc_str_t`.
pub fn read_pc_str_t(source: &[u8]) -> Result<&[u8], ProgramError> {
    if source.is_empty() {
        Err(ProgramError::InvalidArgument)
    } else {
        let tag_len: usize = try_convert(source[0])?;
        if tag_len + 1 > source.len() {
            Err(ProgramError::InvalidArgument)
        } else {
            Ok(&source[..(1 + tag_len)])
        }
    }
}

fn valid_writable_account(program_id: &Pubkey, account: &AccountInfo) -> bool {
    account.is_writable
        && account.owner == program_id
        && Rent::default().is_exempt(account.lamports(), account.data_len())
}

pub fn check_valid_writable_account(
    program_id: &Pubkey,
    account: &AccountInfo,
) -> Result<(), ProgramError> {
    pyth_assert(
        valid_writable_account(program_id, account),
        OracleError::InvalidWritableAccount.into(),
    )
}

/// Checks whether this instruction is trying to update an individual publisher's price (`true`) or
/// is only trying to refresh the aggregate (`false`)
pub fn is_component_update(cmd_args: &UpdPriceArgs) -> Result<bool, OracleError> {
    match OracleCommand::from_i32(cmd_args.header.command)
        .ok_or(OracleError::UnrecognizedInstruction)?
    {
        OracleCommand::UpdPrice | OracleCommand::UpdPriceNoFailOnError => Ok(true),
        _ => Ok(false),
    }
}
