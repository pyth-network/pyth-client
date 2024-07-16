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
    solana_sdk::pubkey::Pubkey,
    std::mem::size_of,
};

// Attempts to validate and access the contents of an account as a PriceAccount.
fn validate_price_account(
    price_account_info: &mut [u8],
) -> Result<&mut PriceAccount, AggregationError> {
    // TODO: don't return error on non-price account?
    pyth_assert(
        price_account_info.len() >= PriceAccount::MINIMUM_SIZE,
        OracleError::AccountTooSmall.into(),
    )
    .map_err(|_| AggregationError::NotPriceFeedAccount)?;

    {
        let account_header = bytemuck::from_bytes::<AccountHeader>(
            &price_account_info[0..size_of::<AccountHeader>()],
        );

        pyth_assert(
            account_header.magic_number == PC_MAGIC
                && account_header.account_type == PriceAccount::ACCOUNT_TYPE,
            OracleError::InvalidAccountHeader.into(),
        )
        .map_err(|_| AggregationError::NotPriceFeedAccount)?;
    }

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
