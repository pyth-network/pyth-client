use solana_program::msg;

use crate::utils::check_confidence_too_big;

use {
    crate::{
        accounts::{
            PriceAccount,
            PriceInfo,
        },
        c_oracle_header::{
            MAX_CI_DIVISOR,
            PC_STATUS_IGNORED,
        },
        deserialize::{
            load,
            load_checked,
        },
        instruction::UpdPriceArgs,
        utils::{
            check_valid_funding_account,
            check_valid_writable_account,
            is_component_update,
            pyth_assert,
            try_convert,
        },
        OracleError,
    },
    solana_program::{
        account_info::AccountInfo,
        clock::Clock,
        entrypoint::ProgramResult,
        program_error::ProgramError,
        pubkey::Pubkey,
        sysvar::Sysvar,
    },
};

#[cfg(target_arch = "bpf")]
#[link(name = "cpyth-bpf")]
extern "C" {
    pub fn c_upd_aggregate(_input: *mut u8, clock_slot: u64, clock_timestamp: i64) -> bool;
    #[allow(unused)]
    pub fn c_upd_twap(_input: *mut u8, nslots: i64);
}

#[cfg(not(target_arch = "bpf"))]
#[link(name = "cpyth-native")]
extern "C" {
    pub fn c_upd_aggregate(_input: *mut u8, clock_slot: u64, clock_timestamp: i64) -> bool;
    #[allow(unused)]
    pub fn c_upd_twap(_input: *mut u8, nslots: i64);
}

/// Publish component price, never returning an error even if the update failed
// account[0] funding account       [signer writable]
// account[1] price account         [writable]
// account[2] sysvar_clock account  []
pub fn upd_price_no_fail_on_error(
    program_id: &Pubkey,
    accounts: &[AccountInfo],
    instruction_data: &[u8],
) -> ProgramResult {
    match upd_price(program_id, accounts, instruction_data) {
        Err(_) => Ok(()),
        Ok(value) => Ok(value),
    }
}

/// Publish component price
// account[0] funding account       [signer writable]
// account[1] price account         [writable]
// account[2] sysvar_clock account  []
pub fn upd_price(
    program_id: &Pubkey,
    accounts: &[AccountInfo],
    instruction_data: &[u8],
) -> ProgramResult {
    let cmd_args = load::<UpdPriceArgs>(instruction_data)?;
    msg!("slot, price : {}, {}", cmd_args.publishing_slot, cmd_args.price);

    let [funding_account, price_account, clock_account] = match accounts {
        [x, y, z] => Ok([x, y, z]),
        [x, y, _, z] => Ok([x, y, z]),
        _ => Err(OracleError::InvalidNumberOfAccounts),
    }?;

    check_valid_funding_account(funding_account)?;
    check_valid_writable_account(program_id, price_account)?;
    // Check clock
    let clock = Clock::from_account_info(clock_account)?;

    let mut publisher_index: usize = 0;
    let latest_aggregate_price: PriceInfo;
    {
        // Verify that symbol account is initialized
        let price_data = load_checked::<PriceAccount>(price_account, cmd_args.header.version)?;

        // Verify that publisher is authorized
        while publisher_index < try_convert::<u32, usize>(price_data.num_)? {
            if price_data.comp_[publisher_index].pub_ == *funding_account.key {
                break;
            }
            publisher_index += 1;
        }
        pyth_assert(
            publisher_index < try_convert::<u32, usize>(price_data.num_)?,
            OracleError::PermissionViolation.into(),
        )?;


        latest_aggregate_price = price_data.agg_;
        let latest_publisher_price = price_data.comp_[publisher_index].latest_;

        // Check that publisher is publishing a more recent price
        pyth_assert(
            !is_component_update(cmd_args)?
                || cmd_args.publishing_slot > latest_publisher_price.pub_slot_,
            ProgramError::InvalidArgument,
        )?;
    }

    // Try to update the aggregate
    #[allow(unused_variables)]
    let mut aggregate_updated = false;
    if clock.slot > latest_aggregate_price.pub_slot_ {
        #[allow(unused_assignments)]
        unsafe {
            aggregate_updated = c_upd_aggregate(
                price_account.try_borrow_mut_data()?.as_mut_ptr(),
                clock.slot,
                clock.unix_timestamp,
            );
        }
    }

    #[cfg(test)]
    // Sma feature disabled in production for now
    {
        use crate::accounts::PythAccount;
        let account_len = price_account.try_data_len()?;
        if aggregate_updated
            && account_len == crate::time_machine_types::PriceAccountWrapper::MINIMUM_SIZE
        {
            let mut price_account = load_checked::<crate::time_machine_types::PriceAccountWrapper>(
                price_account,
                cmd_args.header.version,
            )?;
            price_account.add_price_to_time_machine()?;
        }
    }


    // Try to update the publisher's price
    if is_component_update(cmd_args)? {
        let mut status: u32 = check_confidence_too_big(cmd_args.price, cmd_args.confidence, cmd_args.status)?;
        
        {
            let mut price_data =
                load_checked::<PriceAccount>(price_account, cmd_args.header.version)?;
            let publisher_price = &mut price_data.comp_[publisher_index].latest_;
            publisher_price.price_ = cmd_args.price;
            publisher_price.conf_ = cmd_args.confidence;
            publisher_price.status_ = status;
            publisher_price.pub_slot_ = cmd_args.publishing_slot;
        }
    }
    msg!("Done updating price");
    Ok(())
}
