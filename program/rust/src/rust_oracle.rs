use solana_program::program_error::ProgramError;
use super::c_entrypoint_wrapper;
use crate::error::OracleResult;
use solana_program::pubkey::Pubkey;
use solana_program::sysvar::slot_history::AccountInfo;
use solana_program::program_memory::sol_memset;
use c_oracle_headers::{pc_map_table_t, SUCCESS};
use std::mem::{size_of, size_of_val};

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
                  valid_funding_account(accounts.get(0)) &&
                  valid_signable_account(program_id, accounts.get(1), sizeof( pc_map_table_t )),
                ERROR_INVALID_ARGUMENT)?;


    let data = accounts.get(1)
      .try_borrow_data()
      .map_err(|_| PythError::InvalidAccountData)?;
    let mapping_account = load::<pc_map_table_t>(*data)?;

    // Check that the account has not already been initialized
    pyth_assert(mapping_account.magic_ == 0 && mapping_account.ver_ == 0, ERROR_INVALID_ARGUMENT)?;

    // Initialize by setting to zero again (just in case) and setting
    // the version number
    let hdr = load::<cmd_hdr_t>(instruction_data);
    sol_memset( data, 0, size_of::<pc_map_table_t>() );
    mapping_account.magic_ = PC_MAGIC;
    mapping_account.ver_   = hdr.ver_;
    mapping_account.type_  = PC_ACCTYPE_MAPPING;
    mapping_account.size_  = size_of::<pc_map_table_t> - size_of_val( mapping_account.prod_ );

    return Result::Ok(SUCCESS);
}

fn pyth_assert(condition: bool, error_code: ProgramError) -> Result<(), ProgramError> {
    return if !condition {
        Result::Err(error_code)
    } else {
        Result::Ok(())
    }
}
