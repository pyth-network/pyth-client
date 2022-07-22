use crate::c_oracle_header::*;
use crate::error::OracleError;
use borsh::BorshDeserialize;
use solana_program::account_info::AccountInfo;
use solana_program::entrypoint::ProgramResult;
use solana_program::msg;
use std::mem::size_of;

pub fn pre_log(instruction_data: &[u8]) -> ProgramResult {
    msg!("Pyth oracle contract");

    let instruction_header: cmd_hdr = cmd_hdr::try_from_slice(&instruction_data[..8])?;
    let instruction_id: u32 = instruction_header
        .cmd_
        .try_into()
        .map_err(|_| OracleError::Generic)?;
    match instruction_id {
        command_t_e_cmd_upd_price => {
            let instruction: cmd_upd_price = cmd_upd_price::try_from_slice(instruction_data)?;
            msg!(
                "Update price: price={:}, conf={:}, status={:}",
                instruction.price_,
                instruction.conf_,
                instruction.status_
            );
        }
        command_t_e_cmd_agg_price => {
            let instruction: cmd_upd_price = cmd_upd_price::try_from_slice(instruction_data)?;
            msg!(
                "Update price: price={:}, conf={:}, status={:}",
                instruction.price_,
                instruction.conf_,
                instruction.status_
            );
        }
        command_t_e_cmd_upd_price_no_fail_on_error => {
            let instruction: cmd_upd_price = cmd_upd_price::try_from_slice(instruction_data)?;
            msg!(
                "Update price no fail on error: price={:}, conf={:}, status={:}",
                instruction.price_,
                instruction.conf_,
                instruction.status_
            );
        }

        command_t_e_cmd_add_mapping => {
            msg!("Add mapping");
        }
        command_t_e_cmd_add_price => {
            msg!("Add price");
        }
        command_t_e_cmd_add_product => {
            msg!("Add product")
        }
        command_t_e_cmd_add_publisher => {
            msg!("Add publisher")
        }
        command_t_e_cmd_del_publisher => {
            msg!("Delete publisher")
        }
        command_t_e_cmd_init_price => {
            msg!("Initialize price")
        }
        command_t_e_cmd_init_mapping => {
            msg!("Initialize mapping account");
        }

        command_t_e_cmd_set_min_pub => {
            msg!("Set minimum number of publishers");
        }
        command_t_e_cmd_upd_product => {
            msg!("Update product");
        }
        _ => {
            msg!("Unrecognized instruction");
            return Err(OracleError::Generic.into());
        }
    }
    Ok(())
}

pub fn post_log(c_ret_val: u64, accounts: &[AccountInfo]) -> ProgramResult {
    if c_ret_val == SUCCESSFULLY_UPDATED_AGGREGATE {
        let start: usize = PRICE_T_AGGREGATE_OFFSET
            .try_into()
            .map_err(|_| OracleError::Generic)?;
        let price_struct: pc_price_info = pc_price_info::try_from_slice(
            &accounts
                .get(1)
                .ok_or(OracleError::Generic)?
                .try_borrow_data()?[start..(start + size_of::<pc_price_info>())],
        )?;
        msg!(
            "Aggregate updated : price={:}, conf={:}, status={:}",
            price_struct.price_,
            price_struct.conf_,
            price_struct.status_
        );
    }
    Ok(())
}
