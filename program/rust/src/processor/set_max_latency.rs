use {
    crate::{
        accounts::PriceAccount,
        deserialize::{
            load,
            load_checked,
        },
        instruction::SetMaxLatencyArgs,
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
        program_error::ProgramError,
        pubkey::Pubkey,
    },
    std::mem::size_of,
};

/// Set max latency
// account[0] funding account       [signer writable]
// account[1] price account         [signer writable]
pub fn set_max_latency(
    program_id: &Pubkey,
    accounts: &[AccountInfo],
    instruction_data: &[u8],
) -> ProgramResult {
    let cmd = load::<SetMaxLatencyArgs>(instruction_data)?; // Loading SetMaxLatencyArgs

    pyth_assert(
        instruction_data.len() == size_of::<SetMaxLatencyArgs>(), // Checking size of SetMaxLatencyArgs
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
        &cmd.header,
    )?;

    let mut price_account_data = load_checked::<PriceAccount>(price_account, cmd.header.version)?;
    price_account_data.max_latency_ = cmd.max_latency;

    Ok(())
}
