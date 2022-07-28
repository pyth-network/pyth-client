use super::c_entrypoint_wrapper;
use crate::c_oracle_header::{
    pc_acc_t, pc_price_t, PC_MAGIC, PC_VERSION, PRICE_ACCOUNT_SIZE, SUCCESSFULLY_UPDATED_AGGREGATE, PRICE_T_TIMESTAMP_OFFSET, PRICE_T_AGGREGATE_CONF_OFFSET, PRICE_T_AGGREGATE_PRICE_OFFSET, PRICE_T_PREV_TIMESTAMP_OFFSET, PRICE_T_PREV_CONF_OFFSET, PRICE_T_PREV_AGGREGATE_OFFSET, EXTRA_PUBLISHER_SPACE
};
use crate::error::OracleError;
use crate::error::OracleResult;
use crate::time_machine_types::TimeMachineWrapper;
use borsh::{BorshDeserialize, BorshSerialize};
use solana_program::entrypoint::ProgramResult;
use solana_program::msg;
use solana_program::program::invoke;
use solana_program::program_error::ProgramError;
use solana_program::pubkey::Pubkey;
use solana_program::rent::Rent;
use solana_program::system_instruction::transfer;
use solana_program::sysvar::slot_history::AccountInfo;
use std::mem::size_of;
use std::result::Result;
use std::cell::Ref;

fn u64_to_usize(x: u64)-> Result<usize, ProgramError>{
    <u64 as TryInto<usize>>::try_into(x).map_err(|_| OracleError::IntegerCastingError.into())
}

fn get_u64_from_offset(offset : u64, data : &Ref<'_, &mut [u8]>) -> OracleResult{
    let u64_size : usize = 8;
    let offset : usize = u64_to_usize(offset)?; 
    u64::try_from_slice(&data[offset..offset + u64_size]).map_err(|_| ProgramError::BorshIoError("Could not parse price t member".to_string()))
}
fn get_tracker_params(price_account: &AccountInfo)->Result<(u64, u64, u64, u64, u64, u64), ProgramError>{
    let data = price_account.try_borrow_data()?;
    let current_time = get_u64_from_offset(PRICE_T_TIMESTAMP_OFFSET, &data)?;
    let prev_time = get_u64_from_offset(PRICE_T_PREV_TIMESTAMP_OFFSET, &data)?;
    let current_price = get_u64_from_offset(PRICE_T_AGGREGATE_PRICE_OFFSET, &data)?;
    let prev_price = get_u64_from_offset(PRICE_T_PREV_AGGREGATE_OFFSET, &data)?;
    let current_conf = get_u64_from_offset(PRICE_T_AGGREGATE_CONF_OFFSET, &data)?;
    let prev_conf = get_u64_from_offset(PRICE_T_PREV_CONF_OFFSET, &data)?;
    

    Ok((current_time, prev_time, current_price, prev_price, current_conf, prev_conf))
}

///Calls the c oracle update_price, and updates the Time Machine if needed
pub fn update_price(
    program_id: &Pubkey,
    accounts: &Vec<AccountInfo>,
    instruction_data: &[u8],
    input: *mut u8,
) -> OracleResult {
    let account_len = accounts[1].try_data_len()?;
    let price_t_with_twap_tracker_len: usize = PRICE_ACCOUNT_SIZE
        .try_into()
        .map_err(|_| OracleError::IntegerCastingError)?;
    let price_t_size = size_of::<pc_price_t>();
    if account_len < price_t_with_twap_tracker_len {
        if account_len != price_t_size {
            return Err(ProgramError::InvalidArgument);
        }
        msg!("Please resize the account to allow for SMA tracking!");
        return c_entrypoint_wrapper(input);
    }
    let c_ret_value = c_entrypoint_wrapper(input)?;
    if c_ret_value == SUCCESSFULLY_UPDATED_AGGREGATE {
        let (current_time, prev_time, current_price, prev_price, current_conf, prev_conf) = get_tracker_params(&accounts[1])?;
        let extra_publisher_space = u64_to_usize(EXTRA_PUBLISHER_SPACE)?;
        let time_machine_size = size_of::<TimeMachineWrapper>();
        let mut time_machine = TimeMachineWrapper::try_from_slice(&accounts[1].try_borrow_mut_data()?[price_t_size + extra_publisher_space .. price_t_size + extra_publisher_space + time_machine_size]).map_err(|_| ProgramError::BorshIoError("Could not parse time machine".to_string()))?;
        time_machine.add_price(current_time, prev_time, current_price, prev_price, current_conf, prev_conf);
        time_machine.serialize(&mut &mut accounts[1].data.borrow_mut()[price_t_size + extra_publisher_space .. price_t_size + extra_publisher_space + time_machine_size])?;
        msg!("updated tracker!");
    }
    Ok(c_ret_value)
}

/// A helper function that upgrades a price account, used by update_version
pub fn upgrade_price_account(
    program_id: &Pubkey,
    accounts: &Vec<AccountInfo>,
    instruction_data: &[u8],
) -> ProgramResult {
    let account_len = accounts[1].try_data_len()?;
    let new_account_len: usize = PRICE_ACCOUNT_SIZE
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
            //transfer lamports if necessary
            if lamports_needed > 0 {
                let transfer_instruction =
                    transfer(accounts[0].key, accounts[1].key, lamports_needed);
                invoke(&transfer_instruction, &accounts[..2])?;
            }
            //resize
            accounts[1].realloc(new_account_len, false)?;
            let (current_time, prev_time, current_price, prev_price, current_conf, prev_conf) = get_tracker_params(&accounts[1])?;
            let extra_publisher_space = u64_to_usize(EXTRA_PUBLISHER_SPACE)?;
            let time_machine_size = size_of::<TimeMachineWrapper>();
            let mut time_machine : TimeMachineWrapper = Default::default();
            time_machine.add_first_price(current_time, current_price, current_conf);
            time_machine.serialize(&mut &mut accounts[1].data.borrow_mut()[price_t_size + extra_publisher_space .. price_t_size + extra_publisher_space + time_machine_size])?;
            Ok(())
        }
        new_account_len => Ok(()),
        _ => Err(ProgramError::InvalidArgument),
    }
}

#[cfg(feature = "resize-account")]
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
        return Err(OracleError::IncorrectMagicNumber.into());
    }

    //update the accounts depending on type
    match pyth_acc_info.type_ {
        PC_ACCTYPE_PRICE => upgrade_price_account(program_id, &accounts, instruction_data)?,
        _ => (),
    }

    //I do not think that we need to have PC_VERSION dependent logic now, but here seems like a good place to
    //do such a thing

    //update the account info
    pyth_acc_info.ver_ = PC_VERSION;
    //TODO: Should I update the size_ field? I am not sure how it is being used, or why it is needed. Is it account_size?
    pyth_acc_info.serialize(&mut &mut accounts[1].data.borrow_mut()[..pyth_acc_info_size])?;
    Ok(0) //SUCCESS
}

#[cfg(not(feature = "resize-account"))]
/// Disabled update_version until pythd is updated
pub fn update_version(
    program_id: &Pubkey,
    accounts: &Vec<AccountInfo>,
    instruction_data: &[u8],
) -> OracleResult {
    //read account info
    panic!("implement me!");
    Ok(0) //SUCCESS
}