//! Error types
use solana_program::program_error::ProgramError;
use std::result::Result;
use thiserror::Error;
// similar to ProgramResult but allows for multiple success values
pub type OracleResult = Result<u64, ProgramError>;

/// Errors that may be returned by the oracle program
#[derive(Clone, Debug, Eq, Error, PartialEq)]
pub enum OracleError {
    /// Generic catch all error
    #[error("Generic")]
    Generic                 = 600,
    /// integer casting error
    #[error("IntegerCastingError")]
    IntegerCastingError     = 601,
    /// c_entrypoint returned an unexpected value
    #[error("UnknownCError")]
    UnknownCError           = 602,
    #[error("UnrecognizedInstruction")]
    UnrecognizedInstruction = 603,
}

impl From<OracleError> for ProgramError {
    fn from(e: OracleError) -> Self {
        ProgramError::Custom(e as u32)
    }
}
