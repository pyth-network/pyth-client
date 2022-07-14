use solana_program::entrypoint::deserialize;
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
    
    let cmd_raw_data : [u8; 8] = instruction_data[..8].try_into().unwrap();

    let cmd_data = unsafe {mem::transmute::<[u8; 8], rust_oracle::c_oracle_header::cmd_hdr>(cmd_raw_data)};

    if cmd_data.ver_ != rust_oracle::c_oracle_header::PC_VERSION{
        //FIXME: add some migration logic might go here?
        panic!("incorrect version numbers");
    }

    match (cmd_data.cmd_ as u32){
        rust_oracle::c_oracle_header::command_t_e_cmd_upd_price | rust_oracle::c_oracle_header::command_t_e_cmd_upd_price_no_fail_on_error => rust_oracle::update_price(program_id, accounts, instruction_data, input), 
        _ => unsafe { return c_entrypoint(input) },
    }
}

solana_program::custom_heap_default!();
solana_program::custom_panic_default!();