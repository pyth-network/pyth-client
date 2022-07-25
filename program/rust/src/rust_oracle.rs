use super::c_entrypoint;
use super::c_oracle_header;
use solana_program::pubkey::Pubkey;
use solana_program::sysvar::slot_history::AccountInfo;

///Calls the c oracle update_price, and updates the Time Machine if needed
pub fn update_price(
    program_id: &Pubkey,
    accounts: &Vec<AccountInfo>,
    instruction_data: &[u8],
    input: *mut u8,
) -> u64 {
    //For now, we did not change the behavior of this. this is just to show the proposed structure of the
    //program
    let c_ret_val = unsafe { c_entrypoint(input) };
    if c_ret_val == c_oracle_header::SUCCESSFULLY_UPDATED_AGGREGATE {
        //0 is the SUCCESS value for solana
        return 0;
    } else {
        return c_ret_val;
    }
}
/// has version number/ account type dependant logic to make sure the given account is compatible
/// with the current version
/// updates the version number for all accounts, and resizes price accounts
pub fn update_version(
    program_id: &Pubkey,
    accounts: &Vec<AccountInfo>,
    instruction_data: &[u8],
) -> u64 {
    panic!("Need to merge fix to pythd in order to implement this");
    0 //SUCCESS
}
