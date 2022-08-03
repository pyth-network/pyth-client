use ::std::mem::size_of;

use borsh::BorshDeserialize;
use solana_program::pubkey::Pubkey;
use solana_program::sysvar::slot_history::AccountInfo;

use crate::c_entrypoint_wrapper;
use crate::c_oracle_header::{
    cmd_hdr,
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
    init_mapping,
    update_price,
    update_version,
};

///dispatch to the right instruction in the oracle
pub fn process_instruction(
    program_id: &Pubkey,
    accounts: &[AccountInfo],
    instruction_data: &[u8],
    input: *mut u8,
) -> OracleResult {
    let cmd_hdr_size = size_of::<cmd_hdr>();
    let cmd_data = cmd_hdr::try_from_slice(&instruction_data[..cmd_hdr_size])?;

    if cmd_data.ver_ != PC_VERSION {
        //FIXME: I am not sure what's best to do here (this is copied from C)
        // it seems to me like we should not break when version numbers change
        //instead we should log a message that asks users to call update_version
        panic!("incorrect version numbers");
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
        command_t_e_cmd_init_mapping => init_mapping(program_id, accounts, instruction_data),
        _ => c_entrypoint_wrapper(input),
    }
}
