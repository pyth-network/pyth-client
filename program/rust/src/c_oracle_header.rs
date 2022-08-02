#![allow(non_camel_case_types)]
#![allow(dead_code)]
//All the custom trait imports should go here
use borsh::{
    BorshDeserialize,
    BorshSerialize,
};
//bindings.rs is generated by build.rs to include
//things defined in bindings.h
include!("../bindings.rs");
