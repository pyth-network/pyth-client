// Allow non upper case globals from C
#![allow(non_upper_case_globals)]
// Allow using the solana_program::entrypoint::deserialize function
#![allow(clippy::not_unsafe_ptr_arg_deref)]

mod c_oracle_header;
mod deserialize;
mod error;
mod log;
mod processor;
mod rust_oracle;
mod time_machine_types;
mod utils;

#[cfg(test)]
mod tests;

use crate::c_oracle_header::SUCCESSFULLY_UPDATED_AGGREGATE;
use crate::error::{
    OracleError,
    OracleResult,
};

use crate::log::{
    post_log,
    pre_log,
};
use processor::process_instruction;

use solana_program::entrypoint::deserialize;
use solana_program::program_error::ProgramError;
use solana_program::{
    custom_heap_default,
    custom_panic_default,
};

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
// Missing safety doc OK because this is just a no-op
#[cfg(not(target_arch = "bpf"))]
#[allow(clippy::missing_safety_doc)]
pub unsafe extern "C" fn c_entrypoint(_input: *mut u8) -> u64 {
    0 //SUCCESS value
}

pub fn c_entrypoint_wrapper(input: *mut u8) -> OracleResult {
    //Throwing an exception from C into Rust is undefined behavior
    //This seems to be the best we can do
    match unsafe { c_entrypoint(input) } {
        0 => Ok(0), // Success
        SUCCESSFULLY_UPDATED_AGGREGATE => Ok(SUCCESSFULLY_UPDATED_AGGREGATE),
        2 => Err(ProgramError::InvalidArgument), //2 is ERROR_INVALID_ARGUMENT in solana_sdk.h
        _ => Err(OracleError::UnknownCError.into()),
    }
}

#[no_mangle]
pub extern "C" fn entrypoint(input: *mut u8) -> u64 {
    let (program_id, accounts, instruction_data) = unsafe { deserialize(input) };

    if let Err(error) = pre_log(&accounts, instruction_data) {
        return error.into();
    }

    let c_ret_val = match process_instruction(program_id, &accounts, instruction_data, input) {
        Err(error) => error.into(),
        Ok(success_status) => success_status,
    };

    if let Err(error) = post_log(c_ret_val, &accounts) {
        return error.into();
    }


    if c_ret_val == SUCCESSFULLY_UPDATED_AGGREGATE {
        //0 is the SUCCESS value for solana
        0
    } else {
        c_ret_val
    }
}

custom_heap_default!();
custom_panic_default!();
