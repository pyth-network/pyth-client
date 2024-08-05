//! Error types
use {
    solana_program::program_error::ProgramError,
    thiserror::Error,
};

/// Errors that may be returned by the oracle program
#[derive(Clone, Debug, Eq, Error, PartialEq)]
pub enum OracleError {
    /// Generic catch all error
    #[error("Generic")]
    Generic                        = 600,
    /// integer casting error
    #[error("IntegerCastingError")]
    IntegerCastingError            = 601,
    /// c_entrypoint returned an unexpected value
    #[error("UnknownCError")]
    UnknownCError                  = 602,
    #[error("UnrecognizedInstruction")]
    UnrecognizedInstruction        = 603,
    #[error("InvalidFundingAccount")]
    InvalidFundingAccount          = 604,
    #[error("InvalidSignableAccount")]
    InvalidSignableAccount         = 605,
    #[error("InvalidSystemAccount")]
    InvalidSystemAccount           = 606,
    #[error("InvalidWritableAccount")]
    InvalidWritableAccount         = 607,
    #[error("InvalidFreshAccount")]
    InvalidFreshAccount            = 608,
    #[error("InvalidInstructionVersion")]
    InvalidInstructionVersion      = 609,
    #[error("InstructionDataTooShort")]
    InstructionDataTooShort        = 610,
    #[error("InstructionDataSliceMisaligned")]
    InstructionDataSliceMisaligned = 611,
    #[error("AccountTooSmall")]
    AccountTooSmall                = 612,
    #[error("DeserializationError")]
    DeserializationError           = 613,
    #[error("FailedAuthenticatingUpgradeAuthority")]
    InvalidUpgradeAuthority        = 614,
    #[error("FailedPdaVerification")]
    InvalidPda                     = 615,
    #[error("InvalidAccountHeader")]
    InvalidAccountHeader           = 616,
    #[error("InvalidNumberOfAccounts")]
    InvalidNumberOfAccounts        = 617,
    #[error("InvalidReadableAccount")]
    InvalidReadableAccount         = 618,
    #[error("PermissionViolation")]
    PermissionViolation            = 619,
    #[error("NeedsSuccesfulAggregation")]
    NeedsSuccesfulAggregation      = 620,
    #[error("MaxLastFeedIndexReached")]
    MaxLastFeedIndexReached        = 621,
    #[error("FeedIndexAlreadyInitialized")]
    FeedIndexAlreadyInitialized    = 622,
}

impl From<OracleError> for ProgramError {
    fn from(e: OracleError) -> Self {
        ProgramError::Custom(e as u32)
    }
}
