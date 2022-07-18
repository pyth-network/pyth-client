use solana_program::entrypoint::deserialize;
use borsh::{BorshDeserialize, BorshSerialize};
use std::mem;
mod rust_oracle;



#[link(name = "cpyth")]
extern "C" {
    fn c_entrypoint(input: *mut u8) -> u64;
}

#[no_mangle]
pub extern "C" fn entrypoint(input: *mut u8) -> u64 {
    
    let (program_id, accounts, instruction_data) = unsafe{deserialize(input)};
    if instruction_data.len() < 8 {
        panic!("not enough data");
    }

    let cmd_data = rust_oracle::c_oracle_header::cmd_hdr::try_from_slice(&instruction_data[..8]).unwrap();
    
    if cmd_data.ver_ != rust_oracle::c_oracle_header::PC_VERSION{
        //FIXME: I am not sure what's best to do here (this is copied from C)
        // it seems to me like we should not break when version numbers change
        //instead we should maintain the update logic accross version in the 
        //upd_account_version command
        panic!("incorrect version numbers");
    }

    match cmd_data.cmd_ as u32{
        rust_oracle::c_oracle_header::command_t_e_cmd_upd_price | rust_oracle::c_oracle_header::command_t_e_cmd_upd_price_no_fail_on_error | rust_oracle::c_oracle_header::command_t_e_cmd_agg_price => rust_oracle::update_price(program_id, accounts, instruction_data, input), 
        rust_oracle::c_oracle_header::command_t_e_cmd_upd_account_version => rust_oracle::update_version(program_id, accounts, instruction_data),
        _ => unsafe { return c_entrypoint(input) },
    }
}

solana_program::custom_heap_default!();
solana_program::custom_panic_default!();