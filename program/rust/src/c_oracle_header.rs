#![allow(non_camel_case_types)]
#![allow(non_snake_case)]
//we do not use all the variables in oracle.h, so this helps with the warnings
#![allow(dead_code)]
//All the custom trait imports should go here
use bytemuck::{
    Pod,
    Zeroable,
};
use solana_program::pubkey::Pubkey;
use std::mem::size_of;
//bindings.rs is generated by build.rs to include
//things defined in bindings.h
include!("../bindings.rs");

/// The PythAccount trait's purpose is to attach constants to the 3 types of accounts that Pyth has
/// (mapping, price, product). This allows less duplicated code, because now we can create generic
/// functions to perform common checks on the accounts and to load and initialize the accounts.
pub trait PythAccount: Pod {
    /// `ACCOUNT_TYPE` is just the account discriminator, it is different for mapping, product and
    /// price
    const ACCOUNT_TYPE: u32;
    /// `INITIAL_SIZE` is the value that the field `size` will take when the account is first
    /// initialized this one is slightly tricky because for mapping (resp. price) `size_` won't
    /// include the unpopulated entries of `prod_` (resp. `comp_`). At the beginning there are 0
    /// products (resp. 0 components) therefore `INITIAL_SIZE` will be equal to the offset of
    /// `prod_` (resp. `comp_`)  Similarly the product account `INITIAL_SIZE` won't include any
    /// key values.
    const INITIAL_SIZE: u32;
    /// `minimum_size()` is the minimum size that the solana account holding the struct needs to
    /// have. `INITIAL_SIZE` <= `minimum_size()`
    fn minimum_size() -> usize {
        size_of::<Self>()
    }
}


#[repr(C)]
#[derive(Copy, Clone, Pod, Zeroable)]
pub struct ProductAccount {
    pub header:              AccountHeader,
    pub first_price_account: CPubkey,
}

impl PythAccount for MappingAccount {
    const ACCOUNT_TYPE: u32 = PC_ACCTYPE_MAPPING;
    const INITIAL_SIZE: u32 = 56;
}

impl PythAccount for ProductAccount {
    const ACCOUNT_TYPE: u32 = PC_ACCTYPE_PRODUCT;
    const INITIAL_SIZE: u32 = size_of::<ProductAccount>() as u32;
    fn minimum_size() -> usize {
        PC_PROD_ACC_SIZE as usize
    }
}

impl PythAccount for PriceAccount {
    const ACCOUNT_TYPE: u32 = PC_ACCTYPE_PRICE;
    const INITIAL_SIZE: u32 = 240;
}

#[repr(C)]
#[derive(Copy, Clone, Pod, Zeroable)]
pub struct PriceAccount {
    pub header:             AccountHeader,
    pub price_type:         u32,      // Type of the price account
    pub exponent:           i32,      // Exponent for the published prices
    pub num_:               u32,      // Current number of authorized publishers
    pub num_qt_:            u32,      // Number of valid quotes for the last aggregation
    pub last_slot_:         u64,      // Last slot with a succesful aggregation (status : TRADING)
    pub valid_slot_:        u64,      // Second to last slot where aggregation was attempted
    pub twap_:              PriceEma, // Ema for price
    pub twac_:              PriceEma, // Ema for confidence
    pub timestamp_:         i64,      // Last time aggregation was attempted
    pub min_pub_:           u8,       // Minimum valid publisher quotes for a succesful aggregation
    pub unused_1_:          i8,
    pub unused_2_:          i16,
    pub unused_3_:          i32,
    pub product_account:    CPubkey,
    pub next_price_account: CPubkey,
    pub prev_slot_:         u64, /* Second to last slot where aggregation was succesful (status
                                  * : TRADING) */
    pub prev_price_:        i64,       // Aggregate price at prev_slot_
    pub prev_conf_:         u64,       // Confidence interval at prev_slot_
    pub prev_timestamp_:    i64,       // Timestamp of prev_slot_
    pub agg_:               PriceInfo, // Last attempted aggregate results
    pub comp_:              [PriceComponent; 32usize], // Publisher's price components
}

#[repr(C)]
#[derive(Copy, Clone, Pod, Zeroable)]
pub struct PriceComponent {
    pub pub_:    CPubkey,
    pub agg_:    PriceInfo,
    pub latest_: PriceInfo,
}

#[repr(C)]
#[derive(Debug, Copy, Clone, Pod, Zeroable)]
pub struct PriceInfo {
    pub price_:           i64,
    pub conf_:            u64,
    pub status_:          u32,
    pub corp_act_status_: u32,
    pub pub_slot_:        u64,
}

#[repr(C)]
#[derive(Debug, Copy, Clone, Pod, Zeroable)]
pub struct PriceEma {
    pub val_:   i64,
    pub numer_: i64,
    pub denom_: i64,
}

#[repr(C)]
#[derive(Copy, Clone, Zeroable, Pod)]
pub struct AccountHeader {
    pub magic_number: u32,
    pub version:      u32,
    pub account_type: u32,
    pub size:         u32,
}

#[repr(C)]
#[derive(Copy, Clone)]
pub struct MappingAccount {
    pub header:               AccountHeader,
    pub number_of_products:   u32,
    pub unused_:              u32,
    pub next_mapping_account: CPubkey,
    pub product_list:         [CPubkey; 640usize],
}

// Unsafe impl because pc_pub_key is a union
unsafe impl Pod for CPubkey {
}
unsafe impl Zeroable for CPubkey {
}


// Unsafe impl because prod_ is of size 640 and there's no derived trait for this size
unsafe impl Pod for MappingAccount {
}
unsafe impl Zeroable for MappingAccount {
}

#[repr(C)]
#[derive(Copy, Clone)]
pub union CPubkey {
    pub k1_: [u8; 32usize],
    pub k8_: [u64; 4usize],
}

impl CPubkey {
    pub fn new_unique() -> CPubkey {
        let solana_unique = Pubkey::new_unique();
        CPubkey {
            k1_: solana_unique.to_bytes(),
        }
    }
}
