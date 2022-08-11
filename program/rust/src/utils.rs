use crate::OracleError;
use solana_program::program_error::ProgramError;

pub fn pyth_assert(condition: bool, error_code: ProgramError) -> Result<(), ProgramError> {
    if !condition {
        Result::Err(error_code)
    } else {
        Result::Ok(())
    }
}

/// Convert `x: T` into a `U`, returning the appropriate `OracleError` if the conversion fails.
pub fn try_convert<T, U: TryFrom<T>>(x: T) -> Result<U, OracleError> {
    // Note: the error here assumes we're only applying this function to integers right now.
    U::try_from(x).map_err(|_| OracleError::IntegerCastingError)
}
