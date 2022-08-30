use bytemuck::{
    Pod,
    Zeroable,
};
use solana_program::pubkey::Pubkey;
use std::mem::size_of;

pub const PYTH_MAGIC_NUMBER: u32 = 0xa1b2c3d4;
pub const PYTH_VERSION: u32 = 2;

pub const ACCOUNT_TYPE_MAPPING: u32 = 1;
pub const ACCOUNT_TYPE_PRODUCT: u32 = 2;
pub const ACCOUNT_TYPE_PRICE: u32 = 3;
#[allow(unused)]
pub const ACCOUNT_TYPE_TEST: u32 = 4;
pub const PRICE_TYPE_UNKNOWN: u32 = 0;
#[allow(unused)]
pub const PRICE_TYPE_PRICE: u32 = 1;


pub const PRICE_STATUS_UNKNOWN: u32 = 0;
#[allow(unused)]
pub const PRICE_STATUS_TRADING: u32 = 1;
#[allow(unused)]
pub const PRICE_STATUS_HALTED: u32 = 2;
#[allow(unused)]
pub const PRICE_STATUS_AUCTION: u32 = 3;

pub const MAX_CI_DIVISOR: i64 = 20;
pub const MAX_NUM_DECIMALS: i32 = 8;

impl PriceAccount {
    pub const MAX_NUMBER_OF_PUBLISHERS: usize = 32;
}

impl MappingAccount {
    pub const MAX_NUMBER_OF_PRODUCTS: usize = 640;
}

/// The PythAccount trait's purpose is to attach constants to the 3 types of accounts that Pyth has
/// (mapping, price, product). This allows less duplicated code, because now we can create generic
/// functions to perform common checks on the accounts and to load and initialize the accounts.
pub trait PythAccount: Pod {
    /// `ACCOUNT_TYPE` is just the account discriminator, it is different for mapping, product and
    /// price
    const ACCOUNT_TYPE: u32;
    /// `INITIAL_SIZE` is the value that the field `size_` will take when the account is first
    /// initialized this one is slightly tricky because for mapping (resp. price) `size_` won't
    /// include the unpopulated entries of `prod_` (resp. `comp_`). At the beginning there are 0
    /// products (resp. 0 components) therefore `INITIAL_SIZE` will be equal to the offset of
    /// `prod_` (resp. `comp_`)  Similarly the product account `INITIAL_SIZE` won't include any
    /// key values.
    const INITIAL_SIZE: u32;
    /// `minimum_size()` is the minimum size that the solana account holding the struct needs to
    /// have. `INITIAL_SIZE` <= `minimum_size()`
    const MINIMUM_SIZE: usize = size_of::<Self>();
}

impl PythAccount for MappingAccount {
    const ACCOUNT_TYPE: u32 = ACCOUNT_TYPE_MAPPING;
    /// Equal to the offset of `prod_` in `MappingAccount`, see the trait comment for more detail
    const INITIAL_SIZE: u32 = 56;
}

impl PythAccount for ProductAccount {
    const ACCOUNT_TYPE: u32 = ACCOUNT_TYPE_PRODUCT;
    const INITIAL_SIZE: u32 = size_of::<ProductAccount>() as u32;
    const MINIMUM_SIZE: usize = 512;
}

impl PythAccount for PriceAccount {
    const ACCOUNT_TYPE: u32 = ACCOUNT_TYPE_PRICE;
    /// Equal to the offset of `comp_` in `PriceAccount`, see the trait comment for more detail
    const INITIAL_SIZE: u32 = 240;
}

#[repr(C)]
#[derive(Copy, Clone, Pod, Zeroable)]
pub struct PriceAccount {
    pub header:             AccountHeader,
    /// Type of the price account
    pub price_type:         u32,
    /// Exponent for the published prices
    pub exponent:           i32,
    /// Current number of authorized publishers
    pub num_:               u32,
    /// Number of valid quotes for the last aggregation
    pub num_qt_:            u32,
    /// Last slot with a succesful aggregation (status : TRADING)
    pub last_slot_:         u64,
    /// Second to last slot where aggregation was attempted
    pub valid_slot_:        u64,
    /// Ema for price
    pub twap_:              PriceEma,
    /// Ema for confidence
    pub twac_:              PriceEma,
    /// Last time aggregation was attempted
    pub timestamp_:         i64,
    /// Minimum valid publisher quotes for a succesful aggregation
    pub min_pub_:           u8,
    pub unused_1_:          i8,
    pub unused_2_:          i16,
    pub unused_3_:          i32,
    /// Corresponding product account
    pub product_account:    Pubkey,
    /// Next price account in the list
    pub next_price_account: Pubkey,
    /// Second to last slot where aggregation was succesful (i.e. status : TRADING)
    pub prev_slot_:         u64,
    /// Aggregate price at prev_slot_
    pub prev_price_:        i64,
    /// Confidence interval at prev_slot_
    pub prev_conf_:         u64,
    /// Timestamp of prev_slot_
    pub prev_timestamp_:    i64,
    /// Last attempted aggregate results
    pub agg_:               PriceInfo,
    /// Publishers' price components
    pub comp_:              [PriceComponent; PriceAccount::MAX_NUMBER_OF_PUBLISHERS],
}

#[repr(C)]
#[derive(Copy, Clone, Pod, Zeroable)]
pub struct PriceComponent {
    pub pub_:    Pubkey,
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
#[derive(Copy, Clone, Pod, Zeroable)]
pub struct ProductAccount {
    pub header:              AccountHeader,
    pub first_price_account: Pubkey,
}

#[repr(C)]
#[derive(Copy, Clone)]
pub struct MappingAccount {
    pub header:               AccountHeader,
    pub number_of_products:   u32,
    pub unused_:              u32,
    pub next_mapping_account: Pubkey,
    pub products_list:        [Pubkey; MappingAccount::MAX_NUMBER_OF_PRODUCTS],
}

// Unsafe impl because product_list is of size 640 and there's no derived trait for this size
unsafe impl Pod for MappingAccount {
}
unsafe impl Zeroable for MappingAccount {
}
