//! Error types

use num_derive::FromPrimitive;
use solana_program::{
    decode_error::DecodeError,
    msg,
    program_error::{PrintProgramError, ProgramError},
};
use thiserror::Error;


/// Errors that may be returned by the oracle program
#[derive(Clone, Debug, Eq, Error, FromPrimitive, PartialEq)]
pub enum OracleError {
    /// Invalid instruction passed to program
    #[error("Generic")]
    Generic = 600, //
}

impl PrintProgramError for OracleError {
    fn print<E>(&self) {
        msg!("ORACLE-ERROR: {}", &self.to_string());
    }
}

impl From<OracleError> for ProgramError {
    fn from(e: OracleError) -> Self {
        ProgramError::Custom(e as u32)
    }
}

impl<T> DecodeError<T> for OracleError {
    fn type_of() -> &'static str {
        "Oracle Error"
    }
}
