use {
    crate::{
        accounts::{
            PriceAccount,
            PriceAccountV2,
            PriceFeedMessage,
            PriceInfo,
            PythAccount,
            TwapMessage,
            UPD_PRICE_WRITE_SEED,
        },
        deserialize::{
            load,
            load_checked,
        },
        instruction::UpdPriceArgs,
        utils::{
            check_valid_funding_account,
            check_valid_writable_account,
            get_status_for_update,
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
        instruction::{
            AccountMeta,
            Instruction,
        },
        program::invoke_signed,
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
    let mut aggregate_updated = false;
    if clock.slot > latest_aggregate_price.pub_slot_ {
        unsafe {
            aggregate_updated = c_upd_aggregate(
                price_account.try_borrow_mut_data()?.as_mut_ptr(),
                clock.slot,
                clock.unix_timestamp,
            );
        }
    }


    let account_len = price_account.try_data_len()?;
    if account_len >= PriceAccountV2::MINIMUM_SIZE {
        let mut price_data =
            load_checked::<PriceAccountV2>(price_account, cmd_args.header.version)?;

        if aggregate_updated {
            price_data.update_price_cumulative()?;
            // We want to send a message every time the aggregate price updates. However, during the migration,
            // not every publisher will necessarily provide the accumulator accounts. The message_sent_ flag
            // ensures that after every aggregate update, the next publisher who provides the accumulator accounts
            // will send the message.
            price_data.message_sent_ = 0;
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

                // anchor discriminator for "global:put_all"
                let discriminator = [212, 225, 193, 91, 151, 238, 20, 93];
                let create_inputs_ix = Instruction::new_with_borsh(
                    *accumulator_accounts.program_id.key,
                    &(discriminator, price_account.key.to_bytes(), message),
                    account_metas,
                );

                let message = vec![
                    PriceFeedMessage::from_price_account(price_account.key, &price_data)
                        .as_bytes()
                        .to_vec(),
                    TwapMessage::from_price_account(price_account.key, &price_data)
                        .as_bytes()
                        .to_vec(),
                ];

                // anchor discriminator for "global:put_all"
                let discriminator = [212, 225, 193, 91, 151, 238, 20, 93];
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


    let mut price_data = load_checked::<PriceAccount>(price_account, cmd_args.header.version)?;

    // Try to update the publisher's price
    if is_component_update(cmd_args)? {
}


// Wrapper struct for the accounts required to add data to the accumulator program.
struct MessageBufferAccounts<'a, 'b: 'a> {
    program_id:          &'a AccountInfo<'b>,
    whitelist:           &'a AccountInfo<'b>,
    oracle_auth_pda:     &'a AccountInfo<'b>,
    message_buffer_data: &'a AccountInfo<'b>,
}
