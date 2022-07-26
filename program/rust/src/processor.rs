use crate::c_entrypoint;
use crate::c_oracle_header::{
    cmd_hdr, command_t_e_cmd_agg_price, command_t_e_cmd_upd_account_version,
    command_t_e_cmd_upd_price, command_t_e_cmd_upd_price_no_fail_on_error, PC_VERSION,
};
use crate::rust_oracle::{update_price, update_version};
use ::std::mem::size_of;
use borsh::{BorshDeserialize, BorshSerialize};
use solana_program::pubkey::Pubkey;
use solana_program::sysvar::slot_history::AccountInfo;

pub fn process_instruction(
    program_id: &Pubkey,
    accounts: &Vec<AccountInfo>,
    instruction_data: &[u8],
    input: *mut u8,
) -> u64 {
    let cmd_hdr_size = size_of::<cmd_hdr>();
    let cmd_data = cmd_hdr::try_from_slice(&instruction_data[..cmd_hdr_size]).unwrap();

    if cmd_data.ver_ != PC_VERSION {
        //FIXME: I am not sure what's best to do here (this is copied from C)
        // it seems to me like we should not break when version numbers change
        //instead we should log a message that asks users to call update_version
        panic!("incorrect version numbers");
    }

    match cmd_data.cmd_ as u32 {
        command_t_e_cmd_upd_price
        | command_t_e_cmd_upd_price_no_fail_on_error
        | command_t_e_cmd_agg_price => {
            update_price(program_id, &accounts, &instruction_data, input)
        }
        command_t_e_cmd_upd_account_version => {
            update_version(program_id, &accounts, &instruction_data)
        }
        _ => unsafe { return c_entrypoint(input) },
    }
}
