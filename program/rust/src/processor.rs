use std::mem::size_of;

use solana_program::program_error::ProgramError;
use solana_program::pubkey::Pubkey;
use solana_program::sysvar::slot_history::AccountInfo;

use crate::c_entrypoint_wrapper;
use crate::c_oracle_header::{
    cmd_hdr,
    command_t_e_cmd_add_mapping,
    command_t_e_cmd_add_price,
    command_t_e_cmd_add_product,
    command_t_e_cmd_add_publisher,
    command_t_e_cmd_agg_price,
    command_t_e_cmd_init_mapping,
    command_t_e_cmd_upd_account_version,
    command_t_e_cmd_upd_price,
    command_t_e_cmd_upd_price_no_fail_on_error,
    PC_VERSION,
};
use crate::error::{
    OracleError,
    OracleResult,
};
use crate::rust_oracle::{
    add_mapping,
    add_price,
    add_product,
    add_publisher,
    init_mapping,
    update_price,
    update_version,
};

use crate::deserialize::load;
///dispatch to the right instruction in the oracle
pub fn process_instruction(
    program_id: &Pubkey,
    accounts: &[AccountInfo],
    instruction_data: &[u8],
    input: *mut u8,
) -> OracleResult {
    let cmd_hdr_size = size_of::<cmd_hdr>();
    if instruction_data.len() < cmd_hdr_size {
        return Err(ProgramError::InvalidArgument);
    }
    let cmd_data = load::<cmd_hdr>(&instruction_data[..cmd_hdr_size])?;

    if cmd_data.ver_ != PC_VERSION {
        //FIXME: I am not sure what's best to do here (this is copied from C)
        // it seems to me like we should not break when version numbers change
        //instead we should log a message that asks users to call update_version
        return Err(ProgramError::InvalidArgument);
    }

    match cmd_data
        .cmd_
        .try_into()
        .map_err(|_| OracleError::IntegerCastingError)?
    {
        command_t_e_cmd_upd_price
        | command_t_e_cmd_upd_price_no_fail_on_error
        | command_t_e_cmd_agg_price => update_price(program_id, accounts, instruction_data, input),
        command_t_e_cmd_upd_account_version => {
            update_version(program_id, accounts, instruction_data)
        }
        command_t_e_cmd_add_price => add_price(program_id, accounts, instruction_data),
        command_t_e_cmd_init_mapping => init_mapping(program_id, accounts, instruction_data),
        command_t_e_cmd_add_mapping => add_mapping(program_id, accounts, instruction_data),
        command_t_e_cmd_add_publisher => add_publisher(program_id, accounts, instruction_data),
        command_t_e_cmd_add_product => add_product(program_id, accounts, instruction_data),
        _ => c_entrypoint_wrapper(input),
    }
}
