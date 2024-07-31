use {
    crate::{
        accounts::{
            AccountHeader,
            PriceAccount,
            PriceAccountFlags,
            PythAccount,
            PythOracleSerialize,
        },
        c_oracle_header::PC_MAGIC,
        error::OracleError,
        processor::{
            c_upd_aggregate,
            c_upd_twap,
        },
        utils::pyth_assert,
    },
    pythnet_sdk::messages::{
        PublisherStakeCap,
        PublisherStakeCapsMessage,
    },
    solana_sdk::{
        program_error::ProgramError,
        pubkey::Pubkey,
    },
    std::{
        cmp::max,
        collections::BTreeMap,
        mem::size_of,
    },
};

// Checks that the account is a PriceAccount from the length and header.
fn check_price_account_header(price_account_info: &[u8]) -> Result<(), ProgramError> {
    pyth_assert(
        price_account_info.len() >= PriceAccount::MINIMUM_SIZE,
        OracleError::AccountTooSmall.into(),
    )?;

    let account_header =
        bytemuck::from_bytes::<AccountHeader>(&price_account_info[0..size_of::<AccountHeader>()]);

    pyth_assert(
        account_header.magic_number == PC_MAGIC
            && account_header.account_type == PriceAccount::ACCOUNT_TYPE,
        OracleError::InvalidAccountHeader.into(),
    )?;

    Ok(())
}

// Attempts to validate and access the contents of an account as a PriceAccount.
fn validate_price_account(
    price_account_info: &mut [u8],
) -> Result<&mut PriceAccount, AggregationError> {
    check_price_account_header(price_account_info)
        .map_err(|_| AggregationError::NotPriceFeedAccount)?;

    let data = bytemuck::from_bytes_mut::<PriceAccount>(
        &mut price_account_info[0..size_of::<PriceAccount>()],
    );
    if !data.flags.contains(PriceAccountFlags::ACCUMULATOR_V2) {
        return Err(AggregationError::V1AggregationMode);
    }
    if !data
        .flags
        .contains(PriceAccountFlags::MESSAGE_BUFFER_CLEARED)
    {
        // We make sure that we don't generate v2 messages while v1 messages are still
        // in the message buffer.
        return Err(AggregationError::V1AggregationMode);
    }

    Ok(data)
}

fn update_aggregate(slot: u64, timestamp: i64, price_account: &mut PriceAccount) {
    // NOTE: c_upd_aggregate must use a raw pointer to price data. We already
    // have the exclusive mut reference so we can simply cast before calling
    // the function.
    let updated = unsafe {
        c_upd_aggregate(
            price_account as *mut PriceAccount as *mut u8,
            slot,
            timestamp,
        )
    };

    // If the aggregate was successfully updated, calculate the difference and update TWAP.
    if updated {
        let agg_diff = (slot as i64) - price_account.prev_slot_ as i64;

        // See comment on unsafe `c_upd_aggregate` call above for details.
        unsafe {
            c_upd_twap(price_account as *mut PriceAccount as *mut u8, agg_diff);
        }

        // We want to send a message every time the aggregate price updates. However, during the migration,
        // not every publisher will necessarily provide the accumulator accounts. The message_sent_ flag
        // ensures that after every aggregate update, the next publisher who provides the accumulator accounts
        // will send the message.
        price_account.message_sent_ = 0;
        price_account.update_price_cumulative();
    }
}

#[derive(Debug, PartialEq, thiserror::Error)]
pub enum AggregationError {
    #[error("NotPriceFeedAccount")]
    NotPriceFeedAccount,
    #[error("V1AggregationMode")]
    V1AggregationMode,
    #[error("AlreadyAggregated")]
    AlreadyAggregated,
}

/// Attempts to read a price account and create a new price aggregate if v2
/// aggregation is enabled on this price account. Modifies `price_account_data` accordingly.
/// Returns messages that should be included in the merkle tree, unless v1 aggregation
/// is still in use.
/// Note that the `messages` may be returned even if aggregation fails for some reason.
pub fn aggregate_price(
    slot: u64,
    timestamp: i64,
    price_account_pubkey: &Pubkey,
    price_account_data: &mut [u8],
) -> Result<[Vec<u8>; 2], AggregationError> {
    let price_account = validate_price_account(price_account_data)?;
    if price_account.agg_.pub_slot_ == slot {
        // Avoid v2 aggregation if v1 aggregation has happened in the same slot
        // (this should normally happen only in the slot that contains the v1->v2 transition).
        return Err(AggregationError::AlreadyAggregated);
    }
    update_aggregate(slot, timestamp, price_account);
    Ok([
        price_account
            .as_price_feed_message(price_account_pubkey)
            .to_bytes(),
        price_account
            .as_twap_message(price_account_pubkey)
            .to_bytes(),
    ])
}

/// Load a price account as read-only, returning `None` if it isn't a valid price account.
fn checked_load_price_account(price_account_info: &[u8]) -> Option<&PriceAccount> {
    check_price_account_header(price_account_info).ok()?;
    Some(bytemuck::from_bytes::<PriceAccount>(
        &price_account_info[0..size_of::<PriceAccount>()],
    ))
}

/// Computes the stake caps for each publisher based on the oracle program accounts provided
/// - `account_datas` - the account datas of the oracle program accounts
/// - `timestamp` - the timestamp to include in the message
/// - `m` - m is the cap per symbol, it gets split among all publishers of the symbol
/// - `z` - when a symbol has less than `z` publishers, each publisher gets a cap of `m/z` (instead of `m/number_of_publishers`). This is to prevent a single publisher from getting a large cap when there are few publishers.
///
/// The stake cap for a publisher is computed as the sum of `m/min(z, number_of_publishers)` for all the symbols the publisher is publishing.
pub fn compute_publisher_stake_caps<'a>(
    account_datas: impl IntoIterator<Item = &'a [u8]>,
    timestamp: i64,
    m: u64,
    z: u64,
) -> Vec<u8> {
    let mut publisher_caps: BTreeMap<Pubkey, u64> = BTreeMap::new(); // BTreeMap to ensure it will be sorted by publisher
    for account in account_datas {
        if let Some(price_account) = checked_load_price_account(account) {
            let cap: u64 = m
                .checked_div(max(u64::from(price_account.num_), z))
                .unwrap_or(0);
            for i in 0..(price_account.num_ as usize) {
                if let Some(pub_) = price_account.comp_.get(i).map(|comp| &comp.pub_) {
                    publisher_caps
                        .entry(*pub_)
                        .and_modify(|e: &mut u64| *e = e.saturating_add(cap))
                        .or_insert(cap);
                }
            }
        }
    }

    PublisherStakeCapsMessage {
        publish_time: timestamp,
        caps:         publisher_caps
            .into_iter()
            .map(|(publisher, cap)| PublisherStakeCap {
                publisher: publisher.to_bytes(),
                cap,
            })
            .collect::<Vec<PublisherStakeCap>>()
            .into(),
    }
    .to_bytes()
}
