use solana_program::entrypoint::deserialize;
use std::mem;
mod rust_oracle;
mod c_oracle_header;

#[link(name = "cpyth")]
extern "C" {
    fn c_entrypoint(input: *mut u8) -> u64;
}

#[no_mangle]
pub extern "C" fn entrypoint(input: *mut u8) -> u64 {
    let (program_id, accounts, instruction_data) = unsafe{deserialize(input)};
    if instruction_data.len() < 8 {
        panic!("too few data");
    }
    let cmd_raw_data : [u8; 8] = instruction_data[..8].try_into().expect("slice with incorrect length");
    let cmd_data = unsafe {mem::transmute::<[u8; 8], c_oracle_header::cmd_hdr>(cmd_raw_data)};
    if cmd_data.ver_ != c_oracle_header::PC_VERSION{
        //FIXME: some migration lobic might go here?
        panic!("incorrect version numbers");
    }
    match cmd_data.cmd_{
        7 => rust_oracle::update_price(input), //rust_oracle::update_price(program_id, accounts, instruction_data),
        _ => unsafe { return c_entrypoint(input); },
    }
    
}
