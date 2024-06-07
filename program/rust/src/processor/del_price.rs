use {
    crate::{
        accounts::{
            PriceAccount,
            ProductAccount,
            PublisherScoresAccount,
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
        program_error::ProgramError,
        pubkey::Pubkey,
    },
};

/// Delete a price account. This function will remove the link between the price account and its
/// corresponding product account, then transfer any SOL in the price account to the funding
/// account. This function can only delete the first price account in the linked list of
// account[0] funding account       [signer writable]
// account[1] product account       [signer writable]
// account[2] price account         [signer writable]
// account[3] scores account        [signer writable]
/// Warning: This function is dangerous and will break any programs that depend on the deleted
/// price account!
pub fn del_price(
    program_id: &Pubkey,
    accounts: &[AccountInfo],
    instruction_data: &[u8],
) -> ProgramResult {
    let (funding_account, product_account, price_account, permissions_account, scores_account) =
        match accounts {
            [x, y, z, p] => Ok((x, y, z, p, None)),
            [x, y, z, p, s] => Ok((x, y, z, p, Some(s))),
            _ => Err(OracleError::InvalidNumberOfAccounts),
        }?;

    let cmd_args = load::<CommandHeader>(instruction_data)?;

    check_valid_funding_account(funding_account)?;
    check_permissioned_funding_account(
        program_id,
        product_account,
        funding_account,
        permissions_account,
        cmd_args,
    )?;
    check_permissioned_funding_account(
        program_id,
        price_account,
        funding_account,
        permissions_account,
        cmd_args,
    )?;

    {
        let mut product_data = load_checked::<ProductAccount>(product_account, cmd_args.version)?;
        let price_data = load_checked::<PriceAccount>(price_account, cmd_args.version)?;
        pyth_assert(
            product_data.first_price_account == *price_account.key,
            ProgramError::InvalidArgument,
        )?;

        pyth_assert(
            price_data.product_account == *product_account.key,
            ProgramError::InvalidArgument,
        )?;

        product_data.first_price_account = price_data.next_price_account;
    }

    // Zero out the balance of the price account to delete it.
    // Note that you can't use the system program's transfer instruction to do this operation, as
    // that instruction fails if the source account has any data.
    let lamports = price_account.lamports();
    **price_account.lamports.borrow_mut() = 0;
    **funding_account.lamports.borrow_mut() += lamports;

    if let Some(scores_account) = scores_account {
        let mut scores_account =
            load_checked::<PublisherScoresAccount>(scores_account, cmd_args.version)?;
        scores_account.del_price(*price_account.key)?;
    }

    Ok(())
}
