#![deny(warnings)]
// Allow non upper case globals from C
#![allow(non_upper_case_globals)]

mod accounts;
mod c_oracle_header;
mod deserialize;
mod error;
mod instruction;
mod processor;
mod time_machine_types;
mod utils;

#[cfg(test)]
mod tests;

#[cfg(feature = "debug")]
mod log;

use {
    crate::error::OracleError,
    processor::process_instruction,
    solana_program::entrypoint,
};

// Below is a high lever description of the rust/c setup.

// As we migrate from C to Rust, our Rust code needs to be able to interact with C
// build-bpf.sh is set up to compile the C code into a two archive files
// contained in `./program/c/target/`
//  - `libcpyth-bpf.a` contains the bpf version for production code
//  - `libcpyth-native.a` contains the systems architecture version for tests

// We also generate bindings for the types and constants in oracle.h (as well as other things
// included in bindings.h), these bindings can be accessed through c_oracle_header.rs
// Bindings allow us to access type definitions, function definitions and constants. In order to
// add traits to the bindings, we use the parser in build.rs. The traits must be defined/included
// at the the top of c_oracle_headers.rs. One of the most important traits we deal are the Borsh
// serialization traits.

entrypoint!(process_instruction);
