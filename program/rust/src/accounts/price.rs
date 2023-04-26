use {
    super::{
        AccountHeader,
        PythAccount,
    },
    crate::{
        c_oracle_header::{
            PC_ACCTYPE_PRICE,
            PC_COMP_SIZE,
            PC_COMP_SIZE_V2,
            PC_MAX_SEND_LATENCY,
            PC_STATUS_TRADING,
        },
        error::OracleError,
    },
    bytemuck::{
        Pod,
        Zeroable,
    },
    solana_program::pubkey::Pubkey,
    std::mem::size_of,
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
    /// Whether the current aggregate price has been sent as a message to the message buffer.
    /// 0 = false, 1 = true. (this is a u8 to make the Pod trait happy)
    pub message_sent_:      u8,
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

/// We are currently out of space in our price accounts. We plan to resize them
/// using the resize_price_account function. Until all the accounts have been resized, all instructions MUST work with either of the two versions.
/// Operations may check the account size to determine whether the old or the new version has been passed.
/// The new price accounts add more publishers and introduce PriceCumulative, a new struct designed to store cumulative sums for computing TWAPs.
#[repr(C)]
#[derive(Copy, Clone, Pod, Zeroable)]
pub struct PriceAccountV2 {
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
    pub extra_comp_:        [PriceComponent; (PC_COMP_SIZE_V2 - PC_COMP_SIZE) as usize], // This space is empty until we update the aggregation to support more pubs
    /// Cumulative sums of aggregative price and confidence used to compute arithmetic moving averages
    pub price_cumulative:   PriceCumulative,
}

impl PriceAccountV2 {
    /// This function gets triggered when there's a succesful aggregation and updates the cumulative sums
    pub fn update_price_cumulative(&mut self) -> Result<(), OracleError> {
        if self.agg_.status_ == PC_STATUS_TRADING {
            self.price_cumulative.update(
                self.agg_.price_,
                self.agg_.conf_,
                self.agg_.pub_slot_.saturating_sub(self.prev_slot_),
            ); // pub_slot should always be >= prev_slot, but we protect ourselves against underflow just in case
            Ok(())
        } else {
            Err(OracleError::NeedsSuccesfulAggregation)
        }
    }
}

// This struct can't overflow since :
// |sum(price * slotgap)| <= sum(|price * slotgap|) <= max(|price|) * sum(slotgap) <= i64::MAX * * current_slot <= i64::MAX * u64::MAX <= i128::MAX
// |sum(conf * slotgap)| <= sum(|conf * slotgap|) <= max(|conf|) * sum(slotgap) <= u64::MAX * current_slot <= u64::MAX * u64::MAX <= u128::MAX
// num_gaps <= current_slot <= u64::MAX
/// Contains cumulative sums of aggregative price and confidence used to compute arithmetic moving averages.
/// Informally the TWAP between time t and time T can be computed as :
/// `(T.price_cumulative.price - t.price_cumulative.price) / (T.agg_.pub_slot_ - t.agg_.pub_slot_)`
#[repr(C)]
#[derive(Copy, Clone, Pod, Zeroable)]
pub struct PriceCumulative {
    pub price:    i128, // Cumulative sum of price * slot_gap
    pub conf:     u128, // Cumulative sum of conf * slot_gap
    pub num_gaps: u64,  // Cumulative number of gaps in the uptime
    pub unused:   u64,  // Padding for alignment
}

impl PriceCumulative {
    pub fn update(&mut self, price: i64, conf: u64, slot_gap: u64) {
        self.price += i128::from(price) * i128::from(slot_gap);
        self.conf += u128::from(conf) * u128::from(slot_gap);
        if slot_gap > PC_MAX_SEND_LATENCY.into() {
            self.num_gaps += 1;
        }
    }
}


#[repr(C)]
#[derive(Copy, Clone, Pod, Zeroable)]
pub struct PriceAccountNew {
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
    pub price_cumulative : PriceCumulative
}



#[repr(C)]
#[derive(Copy, Clone, Pod, Zeroable)]
pub struct PriceCumulative{
    pub price : i128,
    pub conf : u128,
    pub num_gaps : u64, // Cumulative number of gaps in the uptime since inception
    pub unused : u64
}

impl PriceCumulative {
    pub fn update(&mut self, price : i64, conf : u64, slot_gap : u64) {
        self.price += i128::from(price) * i128::from(slot_gap);
        self.conf += u128::from(conf) * u128::from(slot_gap);
        if slot_gap > PC_MAX_SEND_LATENCY.into() {
            self.num_gaps += 1;
        }
    }
}

#[repr(C)]
#[derive(Copy, Clone, Pod, Zeroable)]
pub struct PriceComponent {
    pub pub_:    Pubkey,
    pub agg_:    PriceInfo,
    pub latest_: PriceInfo,
}

#[repr(C)]
#[derive(Copy, Clone, Pod, Zeroable)]
pub struct PriceInfo {
    pub price_:           i64,
    pub conf_:            u64,
    pub status_:          u32,
    pub corp_act_status_: u32,
    pub pub_slot_:        u64,
}

#[repr(C)]
#[derive(Copy, Clone, Pod, Zeroable)]
pub struct PriceEma {
    pub val_:   i64,
    pub numer_: i64,
    pub denom_: i64,
}

impl PythAccount for PriceAccount {
    const ACCOUNT_TYPE: u32 = PC_ACCTYPE_PRICE;
    const INITIAL_SIZE: u32 = size_of::<PriceAccount>() as u32;
}

impl PythAccount for PriceAccountV2 {
    const ACCOUNT_TYPE: u32 = PC_ACCTYPE_PRICE;
    const INITIAL_SIZE: u32 = size_of::<PriceAccountV2>() as u32;
}

/// Message format for sending data to other chains via the accumulator program
/// When serialized, each message starts with a unique 1-byte discriminator, followed by the
/// serialized struct data in the definition(s) below.
///
/// Messages are forward-compatible. You may add new fields to messages after all previously
/// defined fields. All code for parsing messages must ignore any extraneous bytes at the end of
/// the message (which could be fields that the code does not yet understand).
#[repr(C)]
#[derive(Debug, Copy, Clone, PartialEq)]
pub struct PriceFeedMessage {
    pub id:                [u8; 32],
    pub price:             i64,
    pub conf:              u64,
    pub exponent:          i32,
    /// The timestamp of this price update in seconds
    pub publish_time:      i64,
    /// The timestamp of the previous price update. This field is intended to allow users to
    /// identify the single unique price update for any moment in time:
    /// for any time t, the unique update is the one such that prev_publish_time < t <= publish_time.
    ///
    /// Note that there may not be such an update while we are migrating to the new message-sending logic,
    /// as some price updates on pythnet may not be sent to other chains (because the message-sending
    /// logic may not have triggered). We can solve this problem by making the message-sending mandatory
    /// (which we can do once publishers have migrated over).
    pub prev_publish_time: i64,
    pub ema_price:         i64,
    pub ema_conf:          u64,
}

impl PriceFeedMessage {
    // The size of the serialized message. Note that this is not the same as the size of the struct
    // (because of the discriminator & struct padding/alignment).
    pub const MESSAGE_SIZE: usize = 1 + 32 + 8 + 8 + 4 + 8 + 8 + 8 + 8;
    pub const DISCRIMINATOR: u8 = 0;

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
            prev_publish_time: account.prev_timestamp_,
            ema_price: account.twap_.val_,
            ema_conf: account.twac_.val_ as u64,
        }
    }

    /// Serialize this message as an array of bytes (including the discriminator)
    /// Note that it would be more idiomatic to return a `Vec`, but that approach adds
    /// to the size of the compiled binary (which is already close to the size limit).
    #[allow(unused_assignments)]
    pub fn as_bytes(&self) -> [u8; PriceFeedMessage::MESSAGE_SIZE] {
        let mut bytes = [0u8; PriceFeedMessage::MESSAGE_SIZE];

        let mut i: usize = 0;

        bytes[i..i + 1].clone_from_slice(&[PriceFeedMessage::DISCRIMINATOR]);
        i += 1;

        bytes[i..i + 32].clone_from_slice(&self.id[..]);
        i += 32;

        bytes[i..i + 8].clone_from_slice(&self.price.to_be_bytes());
        i += 8;

        bytes[i..i + 8].clone_from_slice(&self.conf.to_be_bytes());
        i += 8;

        bytes[i..i + 4].clone_from_slice(&self.exponent.to_be_bytes());
        i += 4;

        bytes[i..i + 8].clone_from_slice(&self.publish_time.to_be_bytes());
        i += 8;

        bytes[i..i + 8].clone_from_slice(&self.prev_publish_time.to_be_bytes());
        i += 8;

        bytes[i..i + 8].clone_from_slice(&self.ema_price.to_be_bytes());
        i += 8;

        bytes[i..i + 8].clone_from_slice(&self.ema_conf.to_be_bytes());
        i += 8;

        bytes
    }
}

impl PythAccount for PriceAccountNew {
    const ACCOUNT_TYPE: u32 = PC_ACCTYPE_PRICE;
    /// Equal to the offset of `comp_` in `PriceAccount`, see the trait comment for more detail
    const INITIAL_SIZE: u32 = PC_PRICE_T_COMP_OFFSET as u32;
}
