use super::c_entrypoint_wrapper;
use crate::error::OracleResult;
use solana_program::pubkey::Pubkey;
use solana_program::sysvar::slot_history::AccountInfo;

///Calls the c oracle update_price, and updates the Time Machine if needed
pub fn update_price(
    program_id: &Pubkey,
    accounts: &Vec<AccountInfo>,
    instruction_data: &[u8],
    input: *mut u8,
) -> OracleResult {
    //For now, we did not change the behavior of this. this is just to show the proposed structure
    // of the program
    c_entrypoint_wrapper(input)
}
/// has version number/ account type dependant logic to make sure the given account is compatible
/// with the current version
/// updates the version number for all accounts, and resizes price accounts
pub fn update_version(
    program_id: &Pubkey,
    accounts: &Vec<AccountInfo>,
    instruction_data: &[u8],
) -> OracleResult {
    panic!("Need to merge fix to pythd in order to implement this");
    Ok(0) //SUCCESS
}
