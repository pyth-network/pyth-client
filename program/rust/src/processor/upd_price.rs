use {
    crate::{
        accounts::{
            Message,
            PriceAccount,
            PriceFeedPayload,
            PriceInfo,
            UPD_PRICE_WRITE_SEED,
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
        hash::hashv,
        instruction::{
            AccountMeta,
            Instruction,
        },
        log::sol_log,
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

    sol_log(&format!("aggregate updated: {}", aggregate_updated));

    if aggregate_updated {
        if let Some(accumulator_accounts) = maybe_accumulator_accounts {
            // Check that the oracle PDA is correctly configured for the program we are calling.
            let oracle_auth_seeds: &[&[u8]] = &[
                UPD_PRICE_WRITE_SEED.as_bytes(),
                &accumulator_accounts.accumulator_program.key.to_bytes(),
            ];
            let (expected_oracle_auth_pda, _) =
                Pubkey::find_program_address(oracle_auth_seeds, program_id);
            pyth_assert(
                expected_oracle_auth_pda == *accumulator_accounts.oracle_auth_pda.key,
                OracleError::InvalidPda.into(),
            )?;

            sol_log("trying to invoke accumulator");

            let account_metas = vec![
                // AccountMeta::new(*funding_account.key, true),
                AccountMeta::new_readonly(*accumulator_accounts.whitelist.key, false),
                AccountMeta::new_readonly(*accumulator_accounts.oracle_auth_pda.key, true),
                AccountMeta::new(*accumulator_accounts.accumulator_data.key, false),
            ];

            let price_data = load_checked::<PriceAccount>(price_account, cmd_args.header.version)?;
            let data_to_send = vec![Message::PriceFeed(PriceFeedPayload::from_price_account(
                price_account.key,
                &price_data,
            ))
            .as_bytes()?];

            // TODO: craft instruction properly
            let discriminator = sighash("global", ACCUMULATOR_UPDATER_IX_NAME);
            let create_inputs_ix = Instruction::new_with_borsh(
                *accumulator_accounts.accumulator_program.key,
                &(discriminator, price_account.key.to_bytes(), data_to_send),
                account_metas,
            );
            sol_log(&format!(
                "invoking CPI with discriminator {:?}",
                discriminator
            ));
            let result = invoke_signed(&create_inputs_ix, accounts, &[oracle_auth_seeds]);

            sol_log(&format!("result: {:?}", result));
        }
    }

    // Try to update the publisher's price
    if is_component_update(cmd_args)? {
        let mut status: u32 = cmd_args.status;
        let mut threshold_conf = cmd_args.price / MAX_CI_DIVISOR;

        if threshold_conf < 0 {
            threshold_conf = -threshold_conf;
        }

        if cmd_args.confidence > try_convert::<_, u64>(threshold_conf)? {
            status = PC_STATUS_IGNORED
        }

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

/// Generate discriminator to be able to call anchor program's ix
/// * `namespace` - "global" for instructions
/// * `name` - name of ix to call CASE-SENSITIVE
///
/// Note: this could probably be converted into a constant hash value
/// since it will always be the same.
pub fn sighash(namespace: &str, name: &str) -> [u8; 8] {
    let preimage = format!("{namespace}:{name}");

    let mut sighash = [0u8; 8];
    sighash.copy_from_slice(&hashv(&[preimage.as_bytes()]).to_bytes()[..8]);
    sighash
}

pub const ACCUMULATOR_UPDATER_IX_NAME: &str = "put_all";


// Wrapper struct for the accounts required to add data to the accumulator program.
struct AccumulatorAccounts<'a, 'b: 'a> {
    accumulator_program: &'a AccountInfo<'b>,
    whitelist:           &'a AccountInfo<'b>,
    oracle_auth_pda:     &'a AccountInfo<'b>,
    accumulator_data:    &'a AccountInfo<'b>,
}
