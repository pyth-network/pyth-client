use {
    crate::{
        accounts::{
            MappingAccount,
            ProductAccount,
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
            try_convert,
        },
        OracleError,
    },
    solana_program::{
        account_info::AccountInfo,
        entrypoint::ProgramResult,
        program_error::ProgramError,
        pubkey::Pubkey,
    },
    std::mem::{
        size_of,
        size_of_val,
    },
};

/// Delete a product account and remove it from the product list of its associated mapping account.
/// The deleted product account must not have any price accounts.
///
/// This function allows you to delete products from non-tail mapping accounts. This ability is a
/// little weird, as it allows you to construct a list of multiple mapping accounts where non-tail
/// accounts have empty space. This is fine however; users should simply add new products to the
/// first available spot.
// key[0] funding account       [signer writable]
// key[1] mapping account       [signer writable]
// key[2] product account       [signer writable]
pub fn del_product(
    program_id: &Pubkey,
    accounts: &[AccountInfo],
    instruction_data: &[u8],
) -> ProgramResult {
    let (funding_account, mapping_account, product_account, permissions_account) = match accounts {
        [w, x, y, p] => Ok((w, x, y, p)),
        _ => Err(OracleError::InvalidNumberOfAccounts),
    }?;

    let cmd_args = load::<CommandHeader>(instruction_data)?;

    check_valid_funding_account(funding_account)?;
    check_permissioned_funding_account(
        program_id,
        mapping_account,
        funding_account,
        permissions_account,
        cmd_args,
    )?;

    check_permissioned_funding_account(
        program_id,
        product_account,
        funding_account,
        permissions_account,
        cmd_args,
    )?;

    {
        let mut mapping_data = load_checked::<MappingAccount>(mapping_account, cmd_args.version)?;
        let product_data = load_checked::<ProductAccount>(product_account, cmd_args.version)?;

        // This assertion is just to make the subtractions below simpler
        pyth_assert(
            mapping_data.number_of_products >= 1,
            ProgramError::InvalidArgument,
        )?;
        pyth_assert(
            product_data.first_price_account == Pubkey::default(),
            ProgramError::InvalidArgument,
        )?;

        let product_key = product_account.key;
        let product_index = mapping_data
            .products_list
            .iter()
            .position(|x| *x == *product_key)
            .ok_or(ProgramError::InvalidArgument)?;

        let num_after_removal: usize = try_convert(
            mapping_data
                .number_of_products
                .checked_sub(1)
                .ok_or(ProgramError::InvalidArgument)?,
        )?;

        let last_key_bytes = mapping_data.products_list[num_after_removal];
        mapping_data.products_list[product_index] = last_key_bytes;
        mapping_data.products_list[num_after_removal] = Pubkey::default();
        mapping_data.number_of_products = try_convert::<_, u32>(num_after_removal)?;
        mapping_data.header.size = try_convert::<_, u32>(
            size_of::<MappingAccount>() - size_of_val(&mapping_data.products_list),
        )? + mapping_data.number_of_products
            * try_convert::<_, u32>(size_of::<Pubkey>())?;
    }

    // Zero out the balance of the price account to delete it.
    // Note that you can't use the system program's transfer instruction to do this operation, as
    // that instruction fails if the source account has any data.
    let lamports = product_account.lamports();
    **product_account.lamports.borrow_mut() = 0;
    **funding_account.lamports.borrow_mut() += lamports;

    Ok(())
}
