use solana_program::{account_info::AccountInfo, pubkey::Pubkey};
use solana_program::msg;
use solana_program::entrypoint::{ ProgramResult};
use borsh::{BorshDeserialize};
use crate::error::OracleError;
use crate::c_oracle_header::{
    cmd_hdr, cmd_upd_price, command_t_e_cmd_add_mapping, command_t_e_cmd_add_price,
    command_t_e_cmd_add_product, command_t_e_cmd_add_publisher, command_t_e_cmd_del_publisher,
    command_t_e_cmd_init_mapping, command_t_e_cmd_init_price, command_t_e_cmd_set_min_pub,
    command_t_e_cmd_upd_price, command_t_e_cmd_upd_price_no_fail_on_error, command_t_e_cmd_agg_price,
    command_t_e_cmd_upd_product
};

pub fn pre_log(
    program_id: &Pubkey,
    accounts: &[AccountInfo],
    instruction_data: &[u8],
) -> ProgramResult {
    msg!("Pyth oracle contract");

    let instruction_header: cmd_hdr = cmd_hdr::try_from_slice(&instruction_data[..8])?;
    let instruction_id: u32 = instruction_header
        .cmd_
        .try_into()
        .map_err(|_| OracleError::Generic)?;
    match instruction_id {
        command_t_e_cmd_upd_price_no_fail_on_error => {
            let instruction: cmd_upd_price =
                cmd_upd_price::try_from_slice(&instruction_data).unwrap();
            msg!(
                "Update price no fail on error: price={:}, conf={:}, status={:}",
                instruction.price_,
                instruction.conf_,
                instruction.status_
            );
        }
        command_t_e_cmd_upd_price => {
            let instruction: cmd_upd_price =
                cmd_upd_price::try_from_slice(&instruction_data).unwrap();
            msg!(
                "Update price: price={:}, conf={:}, status={:}",
                instruction.price_,
                instruction.conf_,
                instruction.status_
            );
        }
        command_t_e_cmd_agg_price => {
            let instruction: cmd_upd_price =
            cmd_upd_price::try_from_slice(&instruction_data).unwrap();
            msg!(
                "Update price: price={:}, conf={:}, status={:}",
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

        command_t_e_cmd_set_min_pub =>{
            msg!("Set minimum number of publishers");
        }
        command_t_e_cmd_upd_product =>{
            msg!("Update product");
        }
        _ => {
            msg!("Unrecognized instruction");
            return Err(OracleError::Generic.into());
        }
    }
    Ok(())
}