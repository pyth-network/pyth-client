use std::mem::{size_of, size_of_val};

use solana_program::program_error::ProgramError;
use solana_program::program_memory::sol_memset;
use solana_program::pubkey::Pubkey;
use solana_program::sysvar::slot_history::AccountInfo;

use crate::c_oracle_header::{pc_map_table_t, PC_MAGIC_V, PC_ACCTYPE_MAPPING_V};
use crate::error::OracleResult;

use super::c_entrypoint_wrapper;

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


pub fn init_mapping(
    program_id: &Pubkey,
    accounts: &Vec<AccountInfo>,
    instruction_data: &[u8],
) -> OracleResult {
    // FIXME: this is an extremely scary way to assert because if you forget the ? it doesn't do anything.
    pyth_assert(accounts.len() == 2 &&
                  valid_funding_account(accounts.get(0).unwrap()) &&
                  valid_signable_account(program_id, accounts.get(1).unwrap(), size_of::<pc_map_table_t>()),
                ProgramError::InvalidArgument)?;

    let data = accounts.get(1)
      .unwrap()
      .try_borrow_data()
      .map_err(|_| ProgramError::InvalidArgument)?;
    let mapping_account = load::<pc_map_table_t>(*data)?;

    // Check that the account has not already been initialized
    pyth_assert(mapping_account.magic_ == 0 && mapping_account.ver_ == 0, ProgramError::InvalidArgument)?;

    // Initialize by setting to zero again (just in case) and setting
    // the version number
    let hdr = load::<cmd_hdr_t>(instruction_data);
    sol_memset( data, 0, size_of::<pc_map_table_t>() );
    mapping_account.magic_ = PC_MAGIC_V;
    mapping_account.ver_   = hdr.ver_;
    mapping_account.type_  = PC_ACCTYPE_MAPPING_V;
    mapping_account.size_  = size_of::<pc_map_table_t>() - size_of_val( mapping_account.prod_ );

    // TODO: bind SUCCESS
    return Result::Ok(0);
}

pub fn pyth_assert(condition: bool, error_code: ProgramError) -> Result<(), ProgramError> {
    return if !condition {
        Result::Err(error_code)
    } else {
        Result::Ok(())
    }
}

pub fn valid_funding_account(account: &AccountInfo) -> bool {
    true
}

pub fn valid_signable_account(program_id: &Pubkey, account: &AccountInfo, expected_size: usize) -> bool {
    true
}