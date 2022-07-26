use super::c_entrypoint_wrapper;
use crate::c_oracle_header::{pc_acc_t, pc_price_t, PC_MAGIC, PC_VERSION, PRICE_ACCOUNT_SIZE};
use crate::error::OracleError;
use crate::error::OracleResult;
use crate::time_machine_types::TimeMachineWrapper;
use borsh::{BorshDeserialize, BorshSerialize};
use solana_program::entrypoint::ProgramResult;
use solana_program::program::invoke;
use solana_program::program_error::ProgramError;
use solana_program::pubkey::Pubkey;
use solana_program::rent::Rent;
use solana_program::system_instruction::transfer;
use solana_program::sysvar::slot_history::AccountInfo;
use std::mem::size_of;

///Calls the c oracle update_price, and updates the Time Machine if needed
pub fn update_price(
    program_id: &Pubkey,
    accounts: &Vec<AccountInfo>,
    instruction_data: &[u8],
    input: *mut u8,
) -> OracleResult {
    //For now, we did not change the behavior of this. this is just to show the proposed structure of the
    //program
    c_entrypoint_wrapper(input)
}

pub fn upgrade_ptice_account(
    program_id: &Pubkey,
    accounts: &Vec<AccountInfo>,
    instruction_data: &[u8],
) -> ProgramResult {
    let account_len = accounts[1].try_data_len()?;
    let new_account_len = PRICE_ACCOUNT_SIZE
        .try_into()
        .map_err(|_| OracleError::IntegerCastingError)?;
    let price_t_size = size_of::<pc_price_t>();
    match account_len {
        price_t_size => {
            //compute the number of lamports needed in the price account to update it
            let rent: Rent = Default::default();
            let lamports_needed: u64 = rent
                .minimum_balance(new_account_len)
                .saturating_sub(accounts[1].lamports());
            //transfer lamports if nescissary
            if lamports_needed > 0 {
                let transfer_instruction =
                    transfer(accounts[0].key, accounts[1].key, lamports_needed);
                invoke(&transfer_instruction, &accounts[..2])?;
            }
            //resize
            accounts[1].realloc(new_account_len, false)?;
            //initialize_tracker();
            Ok(())
        }
        new_account_len => Ok(()),
        _ => Err(ProgramError::InvalidArgument),
    }
}

/// has version number/ account type dependant logic to make sure the given account is compatible
/// with the current version
/// updates the version number for all accounts, and resizes price accounts
pub fn update_version(
    program_id: &Pubkey,
    accounts: &Vec<AccountInfo>,
    instruction_data: &[u8],
) -> OracleResult {
    //read account info
    let pyth_acc_info_size = size_of::<pc_acc_t>();
    let mut pyth_acc_info =
        pc_acc_t::try_from_slice(&accounts[1].try_borrow_data()?[..pyth_acc_info_size])?;

    //check magic number
    if pyth_acc_info.magic_ != PC_MAGIC {
        //TODO: once we settle on error stuff fix this
        panic!("incorrect magic number");
    }

    //update the accounts depending on type
    match pyth_acc_info.type_ {
        PC_ACCTYPE_PRICE => upgrade_ptice_account(program_id, &accounts, instruction_data)?,
        _ => (),
    }

    //I do not think that we need to have PC_VERSION dependent logic now, but here seems like a good place to
    //do such a thing

    //update the account info
    pyth_acc_info.ver_ = PC_VERSION;
    //TODO: Should I update the size_ field? I am not sure how it is being used, or why it is needed. Is it account_size?
    pyth_acc_info
        .clone()
        .serialize(&mut &mut accounts[1].data.borrow_mut()[..pyth_acc_info_size])?;
    Ok(0) //SUCCESS
}
