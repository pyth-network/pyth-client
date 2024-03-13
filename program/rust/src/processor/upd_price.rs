#[cfg(feature = "pythnet")]
use {
    crate::accounts::{
        PythOracleSerialize,
        UPD_PRICE_WRITE_SEED,
    },
    solana_program::instruction::{
        AccountMeta,
        Instruction,
    },
    solana_program::program::invoke_signed,
};
use {
    crate::{
        accounts::{
            PriceAccount,
            PriceInfo,
        },
        deserialize::{
            load,
            load_checked,
        },
        instruction::UpdPriceArgs,
        utils::{
            check_valid_funding_account,
            check_valid_writable_account,
            get_status_for_conf_price_ratio,
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
    #[cfg(feature = "pythnet")]
    pub fn c_upd_aggregate_pythnet(_input: *mut u8, clock_slot: u64, clock_timestamp: i64) -> bool;
    #[cfg(not(feature = "pythnet"))]
    pub fn c_upd_aggregate_solana(_input: *mut u8, clock_slot: u64, clock_timestamp: i64) -> bool;

    #[allow(unused)]
    pub fn c_upd_twap(_input: *mut u8, nslots: i64);
}

#[cfg(not(target_arch = "bpf"))]
#[link(name = "cpyth-native")]
extern "C" {
    #[cfg(feature = "pythnet")]
    pub fn c_upd_aggregate_pythnet(_input: *mut u8, clock_slot: u64, clock_timestamp: i64) -> bool;
    #[cfg(not(feature = "pythnet"))]
    pub fn c_upd_aggregate_solana(_input: *mut u8, clock_slot: u64, clock_timestamp: i64) -> bool;

    #[allow(unused)]
    pub fn c_upd_twap(_input: *mut u8, nslots: i64);
}

#[inline]
pub unsafe fn c_upd_aggregate(input: *mut u8, clock_slot: u64, clock_timestamp: i64) -> bool {
    #[cfg(feature = "pythnet")]
    return c_upd_aggregate_pythnet(input, clock_slot, clock_timestamp);
    #[cfg(not(feature = "pythnet"))]
    return c_upd_aggregate_solana(input, clock_slot, clock_timestamp);
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

/// Update a publisher's price for the provided product. If this update is
/// the first update in a slot, this operation will also trigger price aggregation
/// and result in a new aggregate price in the account.
///
/// account[0] the publisher's account (funds the tx) [signer writable]
///            fails if the publisher's public key is not permissioned for the price account.
/// account[1] the price account [writable]
/// account[2] sysvar clock account []
///
/// The remaining accounts are *optional*. If provided, they cause this instruction to send a
/// message containing the price data to the indicated program via CPI. This program is supposed
/// to be the message buffer program, but it is caller-controlled.
///
/// account[3] program id []
/// account[4] whitelist []
/// account[5] oracle program PDA derived from seeds ["upd_price_write", program_id (account[3])].
///            This PDA will sign the CPI call. Note that this is a different PDA per invoked program,
///            which allows the called-into program to authenticate that it is being invoked by the oracle
///            program. []
/// account[6] message buffer data [writable]
pub fn upd_price(
    program_id: &Pubkey,
    accounts: &[AccountInfo],
    instruction_data: &[u8],
) -> ProgramResult {
    let cmd_args = load::<UpdPriceArgs>(instruction_data)?;

    #[allow(unused_variables)]
    let (funding_account, price_account, clock_account, maybe_accumulator_accounts) = match accounts
    {
        [x, y, z] => Ok((x, y, z, None)),
        // Note: this version of the instruction exists for backward compatibility when publishers were including a
        // now superfluous account in the instruction.
        [x, y, _, z] => Ok((x, y, z, None)),
        [x, y, z, a, b, c, d] => Ok((
            x,
            y,
            z,
            Some(MessageBufferAccounts {
                program_id:          a,
                whitelist:           b,
                oracle_auth_pda:     c,
                message_buffer_data: d,
            }),
        )),
        _ => Err(OracleError::InvalidNumberOfAccounts),
    }?;

    check_valid_funding_account(funding_account)?;
    check_valid_writable_account(program_id, price_account)?;
    // Check clock
    let clock = Clock::from_account_info(clock_account)?;

    let mut publisher_index: usize = 0;
    let latest_aggregate_price: PriceInfo;

    // The price_data borrow happens in a scope because it must be
    // dropped before we borrow again as raw data pointer for the C
    // aggregation logic.
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
                || (cmd_args.publishing_slot > latest_publisher_price.pub_slot_
                    && cmd_args.publishing_slot <= clock.slot),
            ProgramError::InvalidArgument,
        )?;
    }

    // get number of slots from last update
    let slots_since_last_update = clock.slot - latest_aggregate_price.pub_slot_;

    // Try to update the aggregate
    #[allow(unused_variables)]
    let aggregate_updated = unsafe {
        c_upd_aggregate(
            price_account.try_borrow_mut_data()?.as_mut_ptr(),
            clock.slot,
            clock.unix_timestamp,
        )
    };

    if aggregate_updated {
        // get price data here
        let mut price_data = load_checked::<PriceAccount>(price_account, cmd_args.header.version)?;
        // check if its the first price update in the slot
        if slots_since_last_update > 0 {
            price_data.prev_twap_ = price_data.twap_;
            price_data.prev_twac_ = price_data.twac_;
        }
        price_data.twap_ = price_data.prev_twap_;
        price_data.twac_ = price_data.prev_twac_;
        unsafe {
            c_upd_twap(
                price_account.try_borrow_mut_data()?.as_mut_ptr(),
                slots_since_last_update as i64, // Ensure slots_since_last_update is cast to i64, as expected by the function signature
            );
        }
    }

    // Reload price data as a struct after c_upd_aggregate() borrow is dropped
    let mut price_data = load_checked::<PriceAccount>(price_account, cmd_args.header.version)?;

    // Feature-gated accumulator-specific code, used only on pythnet/pythtest
    #[cfg(feature = "pythnet")]
    {
        // We want to send a message every time the aggregate price updates. However, during the migration,
        // not every publisher will necessarily provide the accumulator accounts. The message_sent_ flag
        // ensures that after every aggregate update, the next publisher who provides the accumulator accounts
        // will send the message.
        if aggregate_updated {
            price_data.message_sent_ = 0;
            price_data.update_price_cumulative()?;
        }

        if let Some(accumulator_accounts) = maybe_accumulator_accounts {
            if price_data.message_sent_ == 0 {
                // Check that the oracle PDA is correctly configured for the program we are calling.
                let oracle_auth_seeds: &[&[u8]] = &[
                    UPD_PRICE_WRITE_SEED.as_bytes(),
                    &accumulator_accounts.program_id.key.to_bytes(),
                ];
                let (expected_oracle_auth_pda, bump) =
                    Pubkey::find_program_address(oracle_auth_seeds, program_id);
                pyth_assert(
                    expected_oracle_auth_pda == *accumulator_accounts.oracle_auth_pda.key,
                    OracleError::InvalidPda.into(),
                )?;

                let account_metas = vec![
                    AccountMeta {
                        pubkey:      *accumulator_accounts.whitelist.key,
                        is_signer:   false,
                        is_writable: false,
                    },
                    AccountMeta {
                        pubkey:      *accumulator_accounts.oracle_auth_pda.key,
                        is_signer:   true,
                        is_writable: false,
                    },
                    AccountMeta {
                        pubkey:      *accumulator_accounts.message_buffer_data.key,
                        is_signer:   false,
                        is_writable: true,
                    },
                ];

                let message = vec![
                    price_data
                        .as_price_feed_message(price_account.key)
                        .to_bytes(),
                    price_data.as_twap_message(price_account.key).to_bytes(),
                ];

                // Append a TWAP message if available

                // anchor discriminator for "global:put_all"
                let discriminator: [u8; 8] = [212, 225, 193, 91, 151, 238, 20, 93];
                let create_inputs_ix = Instruction::new_with_borsh(
                    *accumulator_accounts.program_id.key,
                    &(discriminator, price_account.key.to_bytes(), message),
                    account_metas,
                );

                let auth_seeds_with_bump: &[&[u8]] = &[
                    UPD_PRICE_WRITE_SEED.as_bytes(),
                    &accumulator_accounts.program_id.key.to_bytes(),
                    &[bump],
                ];

                invoke_signed(&create_inputs_ix, accounts, &[auth_seeds_with_bump])?;
                price_data.message_sent_ = 1;
            }
        }
    }

    // Try to update the publisher's price
    if is_component_update(cmd_args)? {
        // IMPORTANT: If the publisher does not meet the price/conf
        // ratio condition, its price will not count for the next
        // aggregate.
        let status: u32 =
            get_status_for_conf_price_ratio(cmd_args.price, cmd_args.confidence, cmd_args.status)?;

        {
            let publisher_price = &mut price_data.comp_[publisher_index].latest_;
            publisher_price.price_ = cmd_args.price;
            publisher_price.conf_ = cmd_args.confidence;
            publisher_price.status_ = status;
            publisher_price.pub_slot_ = cmd_args.publishing_slot;
        }
    }

    Ok(())
}

#[allow(dead_code)]
// Wrapper struct for the accounts required to add data to the accumulator program.
struct MessageBufferAccounts<'a, 'b: 'a> {
    program_id:          &'a AccountInfo<'b>,
    whitelist:           &'a AccountInfo<'b>,
    oracle_auth_pda:     &'a AccountInfo<'b>,
    message_buffer_data: &'a AccountInfo<'b>,
}
