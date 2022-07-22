mod c_oracle_header;
mod time_machine_types;
mod error;
mod log;

use crate::log::{post_log, pre_log};
use solana_program::entrypoint::deserialize;

//Below is a high lever description of the rust/c setup.

//As we migrate from C to Rust, our Rust code needs to be able to interract with C
//build-bpf.sh is set up to compile the C code into a bpf archive file, that build.rs
//is set up to link the rust targets to. This enables to interact with the c_entrypoint
//as well as similarly declare other C functions in Rust and call them

//We also generate bindings for the types and constants in oracle.h (as well as other things
//included in bindings.h), these bindings can be accessed through c_oracle_header.rs
//Bindings allow us to access type definitions, function definitions and constants. In order to
//add traits to the bindings, we use the parser in build.rs. The traits must be defined/included
//at the the top of c_oracle_headers.rs. One of the most important traits we deal are the Borsh
//serialization traits.

//the only limitation of our set up is that we can not unit test in rust, anything that calls
//a c function. Though we can test functions that use constants/types defined in oracle.h

//Below is a high lever description of the rust/c setup.

//As we migrate from C to Rust, our Rust code needs to be able to interract with C
//build-bpf.sh is set up to compile the C code into a bpf archive file, that build.rs
//is set up to link the rust targets to. This enables to interact with the c_entrypoint
//as well as similarly declare other C functions in Rust and call them

//We also generate bindings for the types and constants in oracle.h (as well as other things
//included in bindings.h), these bindings can be accessed through c_oracle_header.rs
//Bindings allow us to access type definitions, function definitions and constants. In order to
//add traits to the bindings, we use the parser in build.rs. The traits must be defined/included
//at the the top of c_oracle_headers.rs. One of the most important traits we deal are the Borsh
//serialization traits.

//the only limitation of our set up is that we can not unit test in rust, anything that calls
//a c function. Though we can test functions that use constants/types defined in oracle.h

//do not link with C during unit tests (which are built in native architecture, unlike libpyth.o)
#[cfg(target_arch = "bpf")]
#[link(name = "cpyth")]
extern "C" {
    fn c_entrypoint(input: *mut u8) -> u64;
}

//make the C entrypoint a no-op when running cargo test
#[cfg(not(target_arch = "bpf"))]
pub extern "C" fn c_entrypoint(input: *mut u8) -> u64 {
    0 //SUCCESS value
}

#[no_mangle]
pub extern "C" fn entrypoint(input: *mut u8) -> u64 {
    let (_program_id, accounts, instruction_data) = unsafe { deserialize(input) };

    match pre_log(instruction_data) {
        Err(error) => return error.into(),
        _ => {}
    }

    let c_ret_val = unsafe { c_entrypoint(input) };

    match post_log(c_ret_val, &accounts) {
        Err(error) => return error.into(),
        _ => {}
    }

    if c_ret_val == c_oracle_header::SUCCESSFULLY_UPDATED_AGGREGATE {
        //0 is the SUCCESS value for solana
        return 0;
    } else {
        return c_ret_val;
    }
}

solana_program::custom_heap_default!();
solana_program::custom_panic_default!();
