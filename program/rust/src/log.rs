use crate::c_oracle_header::*;
use crate::error::OracleError;
use borsh::BorshDeserialize;
use solana_program::account_info::AccountInfo;
use solana_program::clock::Clock;
use solana_program::entrypoint::ProgramResult;
use solana_program::msg;
use solana_program::sysvar::Sysvar;
use std::mem::size_of;

pub fn pre_log(accounts: &[AccountInfo], instruction_data: &[u8]) -> ProgramResult {
    msg!("Pyth oracle contract");

    let instruction_header: cmd_hdr = cmd_hdr::try_from_slice(&instruction_data[..8])?;
    let instruction_id: u32 = instruction_header
        .cmd_
        .try_into()
        .map_err(|_| OracleError::Generic)?;

    let clock = Clock::get()?;

    let expo_start : usize = PRICE_T_EXPO_OFFSET.try_into()
    .map_err(|_| OracleError::Generic)?;

    let expo: i32 = i32::try_from_slice(
        &accounts
            .get(1)
            .ok_or(OracleError::Generic)?
            .try_borrow_data()?[expo_start..(expo_start + size_of::<i32>())],
    )?;

    match instruction_id {
        command_t_e_cmd_upd_price | command_t_e_cmd_agg_price => {
            let instruction: cmd_upd_price = cmd_upd_price::try_from_slice(instruction_data)?;
            msg!(
                "UpdatePrice: publisher={:}, price_account={:}, price={:}, conf={:}, expo={:}, status={:}, slot={:}, solana_time={:}",
                accounts.get(0)
                .ok_or(OracleError::Generic)?.key,
                accounts.get(1)
                .ok_or(OracleError::Generic)?.key,
                instruction.price_,
                instruction.conf_,
                expo,
                instruction.status_,
                instruction.pub_slot_,
                clock.unix_timestamp
            );
        }
        command_t_e_cmd_upd_price_no_fail_on_error => {
            let instruction: cmd_upd_price = cmd_upd_price::try_from_slice(instruction_data)?;
            msg!(
                "UpdatePriceNoFailOnError: publisher={:}, price_account={:}, price={:}, conf={:}, expo={:}, status={:}, slot={:}, solana_time={:}",
                accounts.get(0)
                .ok_or(OracleError::Generic)?.key,
                accounts.get(1)
                .ok_or(OracleError::Generic)?.key,
                instruction.price_,
                instruction.conf_,
                expo,
                instruction.status_,
                instruction.pub_slot_,
                clock.unix_timestamp
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
            return Err(OracleError::Generic.into());
        }
    }
    Ok(())
}

pub fn post_log(c_ret_val: u64, accounts: &[AccountInfo]) -> ProgramResult {
    if c_ret_val == SUCCESSFULLY_UPDATED_AGGREGATE {
        let aggregate_price_start: usize = PRICE_T_AGGREGATE_OFFSET
            .try_into()
            .map_err(|_| OracleError::Generic)?;
        // We trust that the C oracle has properly checked this account
        let aggregate_price_info: pc_price_info = pc_price_info::try_from_slice(
            &accounts
                .get(1)
                .ok_or(OracleError::Generic)?
                .try_borrow_data()?[aggregate_price_start..(aggregate_price_start + size_of::<pc_price_info>())],
        )?;

        let ema_start: usize = PRICE_T_EMA_OFFSET
            .try_into()
            .map_err(|_| OracleError::Generic)?;

        let ema_info: pc_ema = pc_ema::try_from_slice(
            &accounts
                .get(1)
                .ok_or(OracleError::Generic)?
                .try_borrow_data()?[ema_start..(ema_start + size_of::<pc_ema>())],
        )?;

        let expo_start : usize = PRICE_T_EXPO_OFFSET.try_into()
        .map_err(|_| OracleError::Generic)?;

        let expo: i32 = i32::try_from_slice(
            &accounts
                .get(1)
                .ok_or(OracleError::Generic)?
                .try_borrow_data()?[expo_start..(expo_start + size_of::<i32>())],
        )?;

        let clock = Clock::get()?;

        msg!(
            "UpdateAggregate : price_account={:}, price={:}, conf={:}, expo={:}, status={:}, slot={:}, solana_time={:}, ema={:}",
            accounts.get(1)
            .ok_or(OracleError::Generic)?.key,
            aggregate_price_info.price_,
            aggregate_price_info.conf_,
            expo,
            aggregate_price_info.status_,
            aggregate_price_info.pub_slot_,
            clock.unix_timestamp,
            ema_info.val_
        );
    }
    Ok(())
}
