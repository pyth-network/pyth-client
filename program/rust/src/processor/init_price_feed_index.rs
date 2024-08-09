use {
    super::reserve_new_price_feed_index,
    crate::{
        accounts::PriceAccount,
        deserialize::{
            load,
            load_checked,
        },
        instruction::CommandHeader,
        utils::{
            check_permissioned_funding_account,
            check_valid_funding_account,
            check_valid_writable_account,
            pyth_assert,
        },
        OracleError,
    },
    solana_program::{
        account_info::AccountInfo,
        entrypoint::ProgramResult,
        program_error::ProgramError,
        pubkey::Pubkey,
    },
    std::mem::size_of,
};

/// Init price feed index
// account[0] funding account        [signer writable]
// account[1] price account          [writable]
// account[2] permissions account    [writable]
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

    let (funding_account, price_account, permissions_account) = match accounts {
        [x, y, p] => Ok((x, y, p)),
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
    check_valid_writable_account(program_id, permissions_account)?;

    let mut price_account_data = load_checked::<PriceAccount>(price_account, cmd.version)?;
    pyth_assert(
        price_account_data.feed_index == 0,
        OracleError::FeedIndexAlreadyInitialized.into(),
    )?;
    price_account_data.feed_index = reserve_new_price_feed_index(permissions_account)?;

    Ok(())
}
