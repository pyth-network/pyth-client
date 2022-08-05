use crate::c_oracle_header::*;
use crate::deserialize::{
    deserialize_single_field_from_account,
    deserialize_single_field_from_buffer,
};
use crate::error::OracleError;
use borsh::BorshDeserialize;
use solana_program::account_info::AccountInfo;
use solana_program::clock::Clock;
use solana_program::entrypoint::ProgramResult;
use solana_program::msg;
use solana_program::program_error::ProgramError;
use solana_program::sysvar::Sysvar;

pub fn pre_log(accounts: &[AccountInfo], instruction_data: &[u8]) -> ProgramResult {
    msg!("Pyth oracle contract");

    let instruction_header: cmd_hdr =
        deserialize_single_field_from_buffer::<cmd_hdr>(instruction_data, None)?;
    let instruction_id: u32 = instruction_header
        .cmd_
        .try_into()
        .map_err(|_| OracleError::IntegerCastingError)?;

    match instruction_id {
        command_t_e_cmd_upd_price | command_t_e_cmd_agg_price => {
            let instruction: cmd_upd_price = cmd_upd_price::try_from_slice(instruction_data)?;
            // Account 1 is price_info in this instruction
            let expo: i32 = deserialize_single_field_from_account::<i32>(
                accounts,
                1,
                Some(PRICE_T_EXPO_OFFSET),
            )?;
            msg!(
                "UpdatePrice: publisher={:}, price_account={:}, price={:}, conf={:}, expo={:}, status={:}, slot={:}, solana_time={:}",
                accounts.get(0)
                .ok_or(ProgramError::NotEnoughAccountKeys)?.key,
                accounts.get(1)
                .ok_or(ProgramError::NotEnoughAccountKeys)?.key,
                instruction.price_,
                instruction.conf_,
                expo,
                instruction.status_,
                instruction.pub_slot_,
                Clock::get()?.unix_timestamp
            );
        }
        command_t_e_cmd_upd_price_no_fail_on_error => {
            let instruction: cmd_upd_price = cmd_upd_price::try_from_slice(instruction_data)?;
            // Account 1 is price_info in this instruction
            let expo: i32 = deserialize_single_field_from_account::<i32>(
                accounts,
                1,
                Some(PRICE_T_EXPO_OFFSET),
            )?;
            msg!(
                "UpdatePriceNoFailOnError: publisher={:}, price_account={:}, price={:}, conf={:}, expo={:}, status={:}, slot={:}, solana_time={:}",
                accounts.get(0)
                .ok_or(ProgramError::NotEnoughAccountKeys)?.key,
                accounts.get(1)
                .ok_or(ProgramError::NotEnoughAccountKeys)?.key,
                instruction.price_,
                instruction.conf_,
                expo,
                instruction.status_,
                instruction.pub_slot_,
                Clock::get()?.unix_timestamp
            );
        }
        command_t_e_cmd_add_mapping => {
            msg!("AddMapping");
        }
        command_t_e_cmd_add_price => {
            msg!("AddPrice");
        }
        command_t_e_cmd_add_product => {
            msg!("AddProduct")
        }
        command_t_e_cmd_add_publisher => {
            msg!("AddPublisher")
        }
        command_t_e_cmd_del_publisher => {
            msg!("DeletePublisher")
        }
        command_t_e_cmd_init_price => {
            msg!("InitializePrice")
        }
        command_t_e_cmd_init_mapping => {
            msg!("InitializeMapping");
        }

        command_t_e_cmd_set_min_pub => {
            msg!("SetMinimumPublishers");
        }
        command_t_e_cmd_upd_product => {
            msg!("UpdateProduct");
        }
        _ => {
            msg!("UnrecognizedInstruction");
            return Err(OracleError::UnrecognizedInstruction.into());
        }
    }
    Ok(())
}

pub fn post_log(c_ret_val: u64, accounts: &[AccountInfo]) -> ProgramResult {
    if c_ret_val == SUCCESSFULLY_UPDATED_AGGREGATE {
        // We trust that the C oracle has properly checked account 1, we can only get here through
        // the update price instructions
        let aggregate_price_info: pc_price_info = deserialize_single_field_from_account::<
            pc_price_info,
        >(
            accounts, 1, Some(PRICE_T_AGGREGATE_OFFSET)
        )?;
        let ema_info: pc_ema =
            deserialize_single_field_from_account::<pc_ema>(accounts, 1, Some(PRICE_T_EMA_OFFSET))?;
        let expo: i32 =
            deserialize_single_field_from_account::<i32>(accounts, 1, Some(PRICE_T_EXPO_OFFSET))?;

        msg!(
            "UpdateAggregate : price_account={:}, price={:}, conf={:}, expo={:}, status={:}, slot={:}, solana_time={:}, ema={:}",
            accounts.get(1)
            .ok_or(ProgramError::NotEnoughAccountKeys)?.key,
            aggregate_price_info.price_,
            aggregate_price_info.conf_,
            expo,
            aggregate_price_info.status_,
            aggregate_price_info.pub_slot_,
            Clock::get()?.unix_timestamp,
            ema_info.val_
        );
    }
    Ok(())
}
