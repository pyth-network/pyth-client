// Allow non upper case globals from C
#![allow(non_upper_case_globals)]

mod accounts;
mod c_oracle_header;
mod deserialize;
mod error;
mod instruction;
mod processor;
mod utils;

#[cfg(any(test, feature = "library"))]
pub mod validator;

#[cfg(feature = "library")]
pub use solana_program;

#[cfg(test)]
mod tests;

#[cfg(feature = "debug")]
mod log;

// When compiled in `library` mode the on-chain definitions provided by this library are
// exported. This is useful when other libraries wish to directly utilise the exact Solana specific
// on-chain definitions of the Pyth program.
//
// While we have `pyth-sdk-rs` which exposes a more friendly interface, this is still useful when a
// downstream user wants to confirm for example that they can compile against the binary interface
// of this program for their specific solana version.
pub use crate::error::OracleError;
#[cfg(feature = "strum")]
pub use accounts::MessageType;
#[cfg(feature = "library")]
pub use accounts::{
    AccountHeader,
    MappingAccount,
    PermissionAccount,
    PriceAccount,
    PriceAccountFlags,
    PriceComponent,
    PriceEma,
    PriceInfo,
    ProductAccount,
    PythAccount,
    PythOracleSerialize,
};
#[cfg(feature = "library")]
pub use {
    processor::find_publisher_index,
    utils::get_status_for_conf_price_ratio,
};
use {
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
