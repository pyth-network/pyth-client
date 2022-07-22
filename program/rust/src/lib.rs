pub mod c_oracle_header;
mod rust_oracle;
mod time_machine_types;
<<<<<<< HEAD
mod error;
mod log;

use crate::log::{post_log, pre_log};
use solana_program::entrypoint::deserialize;
=======
use borsh::{BorshDeserialize, BorshSerialize};
use solana_program::entrypoint::deserialize;
use solana_program::pubkey::Pubkey;
use solana_program::sysvar::slot_history::AccountInfo;

>>>>>>> 4656a2b (add a new insctruction and interecepted update price calls)

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

    match pre_log(&accounts,instruction_data) {
        Err(error) => return error.into(),
        _ => {}
    }

    let cmd_hdr_size = ::std::mem::size_of::<c_oracle_header::cmd_hdr>();
    if instruction_data.len() < cmd_hdr_size {
        panic!("insufficient data, could not parse instruction");
    }

    let cmd_data = c_oracle_header::cmd_hdr::try_from_slice(&instruction_data[..cmd_hdr_size]).unwrap();

    if cmd_data.ver_ != c_oracle_header::PC_VERSION {
        //FIXME: I am not sure what's best to do here (this is copied from C)
        // it seems to me like we should not break when version numbers change
        //instead we should maintain the update logic accross version in the
        //upd_account_version command
        panic!("incorrect version numbers");
    }

    let c_ret_val = match cmd_data.cmd_ as u32 {
        c_oracle_header::command_t_e_cmd_upd_price
        | c_oracle_header::command_t_e_cmd_upd_price_no_fail_on_error
        | c_oracle_header::command_t_e_cmd_agg_price => {
            rust_oracle::update_price(program_id, accounts, instruction_data, input)
        }
        c_oracle_header::command_t_e_cmd_upd_account_version => {
            rust_oracle::update_version(program_id, accounts, instruction_data)
        }
        _ => unsafe { return c_entrypoint(input) },
    }

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
