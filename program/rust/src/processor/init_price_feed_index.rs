use {
    crate::{
        accounts::{
            PermissionAccount,
            PriceAccount,
        },
        deserialize::{
            load,
            load_checked,
        },
        instruction::CommandHeader,
        utils::{
            check_permissioned_funding_account,
            check_valid_funding_account,
            pyth_assert,
        },
        OracleError,
    },
    solana_program::{
        account_info::AccountInfo,
        entrypoint::ProgramResult,
        program::invoke,
        program_error::ProgramError,
        pubkey::Pubkey,
        rent::Rent,
        system_instruction,
        sysvar::Sysvar,
    },
    std::mem::size_of,
};

/// Init price feed index
// account[0] funding account       [signer writable]
// account[1] price account         [writable]
// account[2] permissions account   [writable]
// account[3] system program account
pub fn init_price_feed_index(
    program_id: &Pubkey,
    accounts: &[AccountInfo],
    instruction_data: &[u8],
) -> ProgramResult {
    let cmd = load::<CommandHeader>(instruction_data)?;

    pyth_assert(
        instruction_data.len() == size_of::<CommandHeader>(),
        ProgramError::InvalidArgument,
    )?;

    let (funding_account, price_account, permissions_account, system_program) = match accounts {
        [x, y, p, z] => Ok((x, y, p, z)),
        _ => Err(OracleError::InvalidNumberOfAccounts),
    }?;

    check_valid_funding_account(funding_account)?;
    check_permissioned_funding_account(
        program_id,
        price_account,
        funding_account,
        permissions_account,
        cmd,
    )?;
    pyth_assert(
        solana_program::system_program::check_id(system_program.key),
        OracleError::InvalidSystemAccount.into(),
    )?;

    if permissions_account.data_len() < PermissionAccount::MIN_SIZE_WITH_LAST_FEED_INDEX {
        let new_size = PermissionAccount::MIN_SIZE_WITH_LAST_FEED_INDEX;
        let rent = Rent::get()?;
        let new_minimum_balance = rent.minimum_balance(new_size);
        let lamports_diff = new_minimum_balance.saturating_sub(permissions_account.lamports());
        if lamports_diff > 0 {
            invoke(
                &system_instruction::transfer(
                    funding_account.key,
                    permissions_account.key,
                    lamports_diff,
                ),
                &[
                    funding_account.clone(),
                    permissions_account.clone(),
                    system_program.clone(),
                ],
            )?;
        }

        permissions_account.realloc(new_size, true)?;
    }
    let mut last_feed_index = PermissionAccount::load_last_feed_index_mut(permissions_account)?;
    *last_feed_index += 1;
    pyth_assert(
        *last_feed_index < (1 << 28),
        OracleError::MaxLastFeedIndexReached.into(),
    )?;

    let mut price_account_data = load_checked::<PriceAccount>(price_account, cmd.version)?;
    price_account_data.feed_index = *last_feed_index;

    Ok(())
}
