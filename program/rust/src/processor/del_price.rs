use {
    crate::{
        accounts::{
            PriceAccount,
            ProductAccount,
        },
        deserialize::{
            load,
            load_checked,
        },
        instruction::CommandHeader,
        utils::{
            check_valid_funding_account,
            check_valid_signable_account_or_permissioned_funding_account,
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
/// price accounts for the given product.
///
/// Warning: This function is dangerous and will break any programs that depend on the deleted
/// price account!
pub fn del_price(
    program_id: &Pubkey,
    accounts: &[AccountInfo],
    instruction_data: &[u8],
) -> ProgramResult {
    let (funding_account, product_account, price_account, permissions_account_option) =
        match accounts {
            [w, x, y] => Ok((w, x, y, None)),
            [w, x, y, p] => Ok((w, x, y, Some(p))),
            _ => Err(OracleError::InvalidNumberOfAccounts),
        }?;

    let cmd_args = load::<CommandHeader>(instruction_data)?;

    check_valid_funding_account(funding_account)?;
    check_valid_signable_account_or_permissioned_funding_account(
        program_id,
        product_account,
        funding_account,
        permissions_account_option,
        cmd_args,
    )?;
    check_valid_signable_account_or_permissioned_funding_account(
        program_id,
        price_account,
        funding_account,
        permissions_account_option,
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

    Ok(())
}
