use {
    crate::{
        accounts::{
            PriceAccount,
            PriceFeedPayload,
            PriceInfo,
            UPD_PRICE_WRITE_SEED,
        },
        deserialize::{
            load,
            load_checked,
        },
        instruction::UpdPriceArgs,
        utils::{
            check_confidence_too_big,
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

    let (funding_account, price_account, clock_account, maybe_accumulator_accounts) = match accounts
    {
        [x, y, z] => Ok((x, y, z, None)),
        [x, y, _, z] => Ok((x, y, z, None)),
        [x, y, z, a, b, c, d] => Ok((
            x,
            y,
            z,
            Some(AccumulatorAccounts {
                accumulator_program: a,
                whitelist:           b,
                oracle_auth_pda:     c,
                accumulator_data:    d,
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

    if aggregate_updated {
        if let Some(accumulator_accounts) = maybe_accumulator_accounts {
            // Check that the oracle PDA is correctly configured for the program we are calling.
            let oracle_auth_seeds: &[&[u8]] = &[
                UPD_PRICE_WRITE_SEED.as_bytes(),
                &accumulator_accounts.accumulator_program.key.to_bytes(),
            ];
            let (expected_oracle_auth_pda, bump) =
                Pubkey::find_program_address(oracle_auth_seeds, program_id);
            pyth_assert(
                expected_oracle_auth_pda == *accumulator_accounts.oracle_auth_pda.key,
                OracleError::InvalidPda.into(),
            )?;

            // 81544

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
                    pubkey:      *accumulator_accounts.accumulator_data.key,
                    is_signer:   false,
                    is_writable: true,
                },
            ];

            // 82376

            let price_data = load_checked::<PriceAccount>(price_account, cmd_args.header.version)?;
            let message =
                vec![
                    PriceFeedPayload::from_price_account(price_account.key, &price_data)
                        .as_bytes()
                        .to_vec(),
                ];

            // 83840

            // anchor discriminator for "global:put_all"
            let discriminator = [212, 225, 193, 91, 151, 238, 20, 93];

            let create_inputs_ix = Instruction::new_with_borsh(
                *accumulator_accounts.accumulator_program.key,
                &(discriminator, price_account.key.to_bytes(), message),
                account_metas,
            );

            // 86312

            let auth_seeds_with_bump: &[&[u8]] = &[
                UPD_PRICE_WRITE_SEED.as_bytes(),
                &accumulator_accounts.accumulator_program.key.to_bytes(),
                &[bump],
            ];

            invoke_signed(&create_inputs_ix, accounts, &[auth_seeds_with_bump])?;

            // 86880
        }
    }


    // Try to update the publisher's price
    if is_component_update(cmd_args)? {
        let status: u32 =
            check_confidence_too_big(cmd_args.price, cmd_args.confidence, cmd_args.status)?;

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

    Ok(())
}


// Wrapper struct for the accounts required to add data to the accumulator program.
struct AccumulatorAccounts<'a, 'b: 'a> {
    accumulator_program: &'a AccountInfo<'b>,
    whitelist:           &'a AccountInfo<'b>,
    oracle_auth_pda:     &'a AccountInfo<'b>,
    accumulator_data:    &'a AccountInfo<'b>,
}
