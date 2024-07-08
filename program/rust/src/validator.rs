use {
    crate::{
        accounts::{AccountHeader, PriceAccount, PriceAccountFlags, PythAccount},
        c_oracle_header::PC_MAGIC,
        error::OracleError,
        processor::{c_upd_aggregate, c_upd_twap},
        utils::pyth_assert,
    },
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
        return Err(AggregationError::LegacyAggregationMode);
    }

    Ok(data)
}

fn update_aggregate(
    slot: u64,
    timestamp: i64,
    price_account: &mut PriceAccount,
) -> Result<(), AggregationError> {
    // NOTE: c_upd_aggregate must use a raw pointer to price data. We already
    // have the exclusive mut reference so we can simply cast before calling
    // the function.
    //
    // TODO: Verify the bytemuck::from_mut_bytes function is as safe as if
    // we called `try_borrow_mut_data()``
    let updated = unsafe {
        c_upd_aggregate(
            price_account as *mut PriceAccount as *mut u8,
            slot,
            timestamp,
        )
    };

    // If the aggregate was successfully updated, calculate the difference
    // and update TWAP.
    if !updated {
        return Ok(());
    }

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
    price_account
        .update_price_cumulative()
        .map_err(|_| AggregationError::NotTradingStatus)?;

    Ok(())
}

#[derive(Debug, thiserror::Error)]
pub enum AggregationError {
    #[error("NotPriceFeedAccount")]
    NotPriceFeedAccount,
    #[error("LegacyAggregationMode")]
    LegacyAggregationMode,
    #[error("NotTradingStatus")]
    NotTradingStatus,
}

pub fn aggregate_price(
    slot: u64,
    timestamp: i64,
    price_account_data: &mut [u8],
) -> Result<(), AggregationError> {
    let price_account = validate_price_account(price_account_data)?;
    update_aggregate(slot, timestamp, price_account)?;
    Ok(())
}
