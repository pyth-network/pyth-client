use {
    crate::{
        accounts::{
            PriceAccount,
            PriceAccountFlags,
            PriceComponent,
            PriceInfo,
            PythOracleSerialize,
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
        instruction::{
            AccountMeta,
            Instruction,
        },
        program::invoke_signed,
        program_error::ProgramError,
        program_memory::sol_memcmp,
        pubkey::Pubkey,
        sysvar::Sysvar,
    },
};

#[cfg(target_arch = "bpf")]
#[link(name = "cpyth-bpf")]
extern "C" {
    pub fn c_upd_aggregate_pythnet(_input: *mut u8, clock_slot: u64, clock_timestamp: i64) -> bool;

    #[allow(unused)]
    pub fn c_upd_twap(_input: *mut u8, nslots: i64);
}

#[cfg(not(target_arch = "bpf"))]
#[link(name = "cpyth-native")]
extern "C" {
    pub fn c_upd_aggregate_pythnet(_input: *mut u8, clock_slot: u64, clock_timestamp: i64) -> bool;

    #[allow(unused)]
    pub fn c_upd_twap(_input: *mut u8, nslots: i64);
}

#[inline]
pub unsafe fn c_upd_aggregate(input: *mut u8, clock_slot: u64, clock_timestamp: i64) -> bool {
    c_upd_aggregate_pythnet(input, clock_slot, clock_timestamp)
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

    let publisher_index: usize;
    let latest_aggregate_price: PriceInfo;
    let flags: PriceAccountFlags;

    // The price_data borrow happens in a scope because it must be
    // dropped before we borrow again as raw data pointer for the C
    // aggregation logic.
    {
        // Verify that symbol account is initialized
        let price_data = load_checked::<PriceAccount>(price_account, cmd_args.header.version)?;

        publisher_index = match find_publisher_index(
            &price_data.comp_[..try_convert::<u32, usize>(price_data.num_)?],
            funding_account.key,
        ) {
            Some(index) => index,
            None => {
                return Err(OracleError::PermissionViolation.into());
            }
        };

        latest_aggregate_price = price_data.agg_;
        let latest_publisher_price = price_data.comp_[publisher_index].latest_;

        // Check that publisher is publishing a more recent price
        pyth_assert(
            !is_component_update(cmd_args)?
                || (cmd_args.publishing_slot > latest_publisher_price.pub_slot_
                    && cmd_args.publishing_slot <= clock.slot),
            ProgramError::InvalidArgument,
        )?;

        flags = price_data.flags;
    }

    if !flags.contains(PriceAccountFlags::ACCUMULATOR_V2) {
        // Try to update the aggregate
        #[allow(unused_variables)]
        if clock.slot > latest_aggregate_price.pub_slot_ {
            let updated = unsafe {
                // NOTE: c_upd_aggregate must use a raw pointer to price
                // data. Solana's `<account>.borrow_*` methods require exclusive
                // access, i.e. no other borrow can exist for the account.
                c_upd_aggregate(
                    price_account.try_borrow_mut_data()?.as_mut_ptr(),
                    clock.slot,
                    clock.unix_timestamp,
                )
            };

            // If the aggregate was successfully updated, calculate the difference and update TWAP.
            if updated {
                let agg_diff = (clock.slot as i64)
                    - load_checked::<PriceAccount>(price_account, cmd_args.header.version)?
                        .prev_slot_ as i64;
                // Encapsulate TWAP update logic in a function to minimize unsafe block scope.
                unsafe {
                    c_upd_twap(price_account.try_borrow_mut_data()?.as_mut_ptr(), agg_diff);
                }
                let mut price_data =
                    load_checked::<PriceAccount>(price_account, cmd_args.header.version)?;
                // We want to send a message every time the aggregate price updates. However, during the migration,
                // not every publisher will necessarily provide the accumulator accounts. The message_sent_ flag
                // ensures that after every aggregate update, the next publisher who provides the accumulator accounts
                // will send the message.
                price_data.message_sent_ = 0;
                price_data.update_price_cumulative();
            }
        }
    }

    // Reload price data as a struct after c_upd_aggregate() borrow is dropped
    let mut price_data = load_checked::<PriceAccount>(price_account, cmd_args.header.version)?;

    // Feature-gated accumulator-specific code, used only on pythnet/pythtest
    let need_message_buffer_update = if flags.contains(PriceAccountFlags::ACCUMULATOR_V2) {
        // We need to clear old messages.
        !flags.contains(PriceAccountFlags::MESSAGE_BUFFER_CLEARED)
    } else {
        // V1
        price_data.message_sent_ == 0
    };

    if need_message_buffer_update {
        if let Some(accumulator_accounts) = maybe_accumulator_accounts {
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

            let message = if flags.contains(PriceAccountFlags::ACCUMULATOR_V2) {
                vec![]
            } else {
                vec![
                    price_data
                        .as_price_feed_message(price_account.key)
                        .to_bytes(),
                    price_data.as_twap_message(price_account.key).to_bytes(),
                ]
            };

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
            if flags.contains(PriceAccountFlags::ACCUMULATOR_V2) {
                price_data
                    .flags
                    .insert(PriceAccountFlags::MESSAGE_BUFFER_CLEARED);
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

/// Find the index of the publisher in the list of components.
///
/// This method first tries to binary search for the publisher's key in the list of components
/// to get the result faster if the list is sorted. If the list is not sorted, it falls back to
/// a linear search.
#[inline(always)]
pub fn find_publisher_index(comps: &[PriceComponent], key: &Pubkey) -> Option<usize> {
    // Verify that publisher is authorized by initially binary searching
    // for the publisher's component in the price account. The binary
    // search might not work if the publisher list is not sorted; therefore
    // we fall back to a linear search.
    let mut binary_search_result = None;

    {
        // Binary search to find the publisher key. Rust std binary search is not used because
        // they guarantee valid outcome only if the array is sorted whereas we want to rely on
        // a Equal match if it is a result on an unsorted array. Currently the rust
        // implementation behaves the same but we do not want to rely on api internals.
        let mut left = 0;
        let mut right = comps.len();
        while left < right {
            let mid = left + (right - left) / 2;
            // sol_memcmp is much faster than rust default comparison of Pubkey. It costs
            // 10CU whereas rust default comparison costs a few times more.
            match sol_memcmp(comps[mid].pub_.as_ref(), key.as_ref(), 32) {
                i if i < 0 => {
                    left = mid + 1;
                }
                i if i > 0 => {
                    right = mid;
                }
                _ => {
                    binary_search_result = Some(mid);
                    break;
                }
            }
        }
    }

    match binary_search_result {
        Some(index) => Some(index),
        None => {
            let mut index = 0;
            while index < comps.len() {
                if sol_memcmp(comps[index].pub_.as_ref(), key.as_ref(), 32) == 0 {
                    break;
                }
                index += 1;
            }
            if index == comps.len() {
                None
            } else {
                Some(index)
            }
        }
    }
}

#[allow(dead_code)]
// Wrapper struct for the accounts required to add data to the accumulator program.
struct MessageBufferAccounts<'a, 'b: 'a> {
    program_id:          &'a AccountInfo<'b>,
    whitelist:           &'a AccountInfo<'b>,
    oracle_auth_pda:     &'a AccountInfo<'b>,
    message_buffer_data: &'a AccountInfo<'b>,
}

#[cfg(test)]
mod test {
    use {
        super::*,
        crate::accounts::PriceComponent,
        quickcheck_macros::quickcheck,
        solana_program::pubkey::Pubkey,
    };

    /// Test the find_publisher_index method works with an unordered list of components.
    #[quickcheck]
    pub fn test_find_publisher_index_unordered_comp(comps: Vec<PriceComponent>) {
        comps.iter().enumerate().for_each(|(idx, comp)| {
            assert_eq!(find_publisher_index(&comps, &comp.pub_), Some(idx));
        });

        let mut key_not_in_list = Pubkey::new_unique();
        while comps.iter().any(|comp| comp.pub_ == key_not_in_list) {
            key_not_in_list = Pubkey::new_unique();
        }

        assert_eq!(find_publisher_index(&comps, &key_not_in_list), None);
    }

    /// Test the find_publisher_index method works with a sorted list of components.
    #[quickcheck]
    pub fn test_find_publisher_index_ordered_comp(mut comps: Vec<PriceComponent>) {
        comps.sort_by_key(|comp| comp.pub_);

        comps.iter().enumerate().for_each(|(idx, comp)| {
            assert_eq!(find_publisher_index(&comps, &comp.pub_), Some(idx));
        });

        let mut key_not_in_list = Pubkey::new_unique();
        while comps.iter().any(|comp| comp.pub_ == key_not_in_list) {
            key_not_in_list = Pubkey::new_unique();
        }

        assert_eq!(find_publisher_index(&comps, &key_not_in_list), None);
    }
}
