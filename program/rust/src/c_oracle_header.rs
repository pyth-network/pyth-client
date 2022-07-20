#![allow(non_upper_case_globals)]
#![allow(non_camel_case_types)]
#![allow(non_snake_case)]
//All the custom trait imports should go here
use borsh::{BorshSerialize, BorshDeserialize};
//bindings.rs is generated by build.rs to include
//things defined in bindings.h
include!("../bindings.rs");