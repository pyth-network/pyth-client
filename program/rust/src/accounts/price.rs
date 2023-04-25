use {
    super::{
        AccountHeader,
        PythAccount,
    },
    crate::c_oracle_header::{
        PC_ACCTYPE_PRICE,
        PC_COMP_SIZE,
        PC_PRICE_T_COMP_OFFSET,
        PC_STATUS_TRADING,
    },
    bytemuck::{
        Pod,
        Zeroable,
    },
    solana_program::{
        program_error::ProgramError,
        pubkey::Pubkey,
    },
};

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
    pub comp_:              [PriceComponent; PC_COMP_SIZE as usize],
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

impl PythAccount for PriceAccount {
    const ACCOUNT_TYPE: u32 = PC_ACCTYPE_PRICE;
    /// Equal to the offset of `comp_` in `PriceAccount`, see the trait comment for more detail
    const INITIAL_SIZE: u32 = PC_PRICE_T_COMP_OFFSET as u32;
}

/// Message format for sending data to other chains via the accumulator program
#[derive(Debug, Copy, Clone)]
#[repr(u8)]
pub enum Message {
    PriceFeed(PriceFeedPayload), // 0
}

impl Message {
    pub fn as_bytes(&self) -> Result<Vec<u8>, ProgramError> {
        let mut bytes = vec![];
        match &self {
            Message::PriceFeed(m) => {
                // discriminant
                bytes.extend_from_slice(&[0u8]);
                // payload
                bytes.extend_from_slice(&m.id[..]);
                bytes.extend_from_slice(&m.price.to_be_bytes());
                bytes.extend_from_slice(&m.conf.to_be_bytes());
                bytes.extend_from_slice(&m.exponent.to_be_bytes());
                bytes.extend_from_slice(&m.publish_time.to_be_bytes());
                bytes.extend_from_slice(&m.ema_price.to_be_bytes());
                bytes.extend_from_slice(&m.ema_conf.to_be_bytes());
            }
        }

        Ok(bytes)
    }
}

#[repr(C)]
#[derive(Debug, Copy, Clone)]
pub struct PriceFeedPayload {
    id:           [u8; 32],
    price:        i64,
    conf:         u64,
    exponent:     i32,
    publish_time: i64,
    ema_price:    i64,
    ema_conf:     u64,
}

impl PriceFeedPayload {
    pub fn from_price_account(key: &Pubkey, account: &PriceAccount) -> Self {
        let (price, conf, publish_time) = if account.agg_.status_ == PC_STATUS_TRADING {
            (account.agg_.price_, account.agg_.conf_, account.timestamp_)
        } else {
            (
                account.prev_price_,
                account.prev_conf_,
                account.prev_timestamp_,
            )
        };

        Self {
            id: key.to_bytes(),
            price,
            conf,
            exponent: account.exponent,
            publish_time,
            ema_price: account.twap_.val_,
            ema_conf: account.twac_.val_ as u64,
        }
    }
}
