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
    command_t_e_cmd_del_publisher,
    command_t_e_cmd_init_mapping,
    command_t_e_cmd_resize_price_account,
    command_t_e_cmd_set_min_pub,
    command_t_e_cmd_upd_price,
    command_t_e_cmd_upd_price_no_fail_on_error,
    command_t_e_cmd_upd_product,
    PC_VERSION,
};
use crate::deserialize::load;
use crate::error::{
    OracleError,
    OracleResult,
};
use crate::rust_oracle::{
    add_mapping,
    add_price,
    add_product,
    add_publisher,
    del_publisher,
    init_mapping,
    resize_price_account,
    init_price,
    set_min_pub,
    upd_product,
    update_price,
};

///dispatch to the right instruction in the oracle
pub fn process_instruction(
    program_id: &Pubkey,
    accounts: &[AccountInfo],
    instruction_data: &[u8],
    input: *mut u8,
) -> OracleResult {
    let cmd_data = load::<cmd_hdr>(instruction_data)?;

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
        command_t_e_cmd_resize_price_account => {
            resize_price_account(program_id, accounts, instruction_data)
        }
        command_t_e_cmd_add_price => add_price(program_id, accounts, instruction_data),
        command_t_e_cmd_init_mapping => init_mapping(program_id, accounts, instruction_data),
        command_t_e_cmd_init_price => init_price(program_id, accounts, instruction_data),
        command_t_e_cmd_add_mapping => add_mapping(program_id, accounts, instruction_data),
        command_t_e_cmd_add_publisher => add_publisher(program_id, accounts, instruction_data),
        command_t_e_cmd_del_publisher => del_publisher(program_id, accounts, instruction_data),
        command_t_e_cmd_add_product => add_product(program_id, accounts, instruction_data),
        command_t_e_cmd_upd_product => upd_product(program_id, accounts, instruction_data),
        command_t_e_cmd_set_min_pub => set_min_pub(program_id, accounts, instruction_data),
        _ => c_entrypoint_wrapper(input),
    }
}
