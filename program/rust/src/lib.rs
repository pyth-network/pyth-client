#![deny(warnings)]
// Allow non upper case globals from C
#![allow(non_upper_case_globals)]

pub mod accounts;
pub mod c_oracle_header;
pub mod deserialize;
pub mod error;
pub mod instruction;
pub mod processor;
pub mod time_machine_types;
pub mod utils;

#[cfg(test)]
mod tests;

#[cfg(feature = "debug")]
mod log;

use {
    crate::error::OracleError,
    processor::process_instruction,
    solana_program::entrypoint,
};

// Below is a high level description of the rust/c setup.

// As we migrate from C to Rust, our Rust code needs to be able to interact with C.
// build-bpf.sh is set up to compile the C code into a two archive files
// contained in `./program/c/target/`
//  - `libcpyth-bpf.a` contains the bpf version for production code
//  - `libcpyth-native.a` contains the systems architecture version for tests

// We also generate bindings for the constants in oracle.h (as well as other things
// included in bindings.h).

entrypoint!(process_instruction);
