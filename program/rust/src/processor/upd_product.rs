use {
    crate::{
        accounts::ProductAccount,
        c_oracle_header::PC_PROD_ACC_SIZE,
        deserialize::{
            load,
            load_checked,
        },
        instruction::CommandHeader,
        utils::{
            check_valid_funding_account,
            check_valid_signable_account_or_permissioned_funding_account,
            pyth_assert,
            read_pc_str_t,
            try_convert,
        },
        OracleError,
    },
    solana_program::{
        account_info::AccountInfo,
        entrypoint::ProgramResult,
        program_error::ProgramError,
        program_memory::sol_memcpy,
        pubkey::Pubkey,
    },
    std::mem::size_of,
};

/// Update the metadata associated with a product, overwriting any existing metadata.
/// The metadata is provided as a list of key-value pairs at the end of the `instruction_data`.
// account[0] funding account       [signer writable]
// account[1] product account       [signer writable]
pub fn upd_product(
    program_id: &Pubkey,
    accounts: &[AccountInfo],
    instruction_data: &[u8],
) -> ProgramResult {
    let (funding_account, product_account, permissions_account_option) = match accounts {
        [x, y] => Ok((x, y, None)),
        [x, y, p] => Ok((x, y, Some(p))),
        _ => Err(OracleError::InvalidNumberOfAccounts),
    }?;

    let hdr = load::<CommandHeader>(instruction_data)?;

    check_valid_funding_account(funding_account)?;
    check_valid_signable_account_or_permissioned_funding_account(
        program_id,
        product_account,
        funding_account,
        permissions_account_option,
        hdr,
    )?;


    {
        // Validate that product_account contains the appropriate account header
        let mut _product_data = load_checked::<ProductAccount>(product_account, hdr.version)?;
    }

    pyth_assert(
        instruction_data.len() >= size_of::<CommandHeader>(),
        ProgramError::InvalidInstructionData,
    )?;
    let new_data_len = instruction_data.len() - size_of::<CommandHeader>();
    let max_data_len = try_convert::<_, usize>(PC_PROD_ACC_SIZE)? - size_of::<ProductAccount>();
    pyth_assert(new_data_len <= max_data_len, ProgramError::InvalidArgument)?;

    let new_data = &instruction_data[size_of::<CommandHeader>()..instruction_data.len()];
    let mut idx = 0;
    // new_data must be a list of key-value pairs, both of which are instances of pc_str_t.
    // Try reading the key-value pairs to validate that new_data is properly formatted.
    while idx < new_data.len() {
        let key = read_pc_str_t(&new_data[idx..])?;
        idx += key.len();
        let value = read_pc_str_t(&new_data[idx..])?;
        idx += value.len();
    }

    // This assertion shouldn't ever fail, but be defensive.
    pyth_assert(idx == new_data.len(), ProgramError::InvalidArgument)?;

    {
        let mut data = product_account.try_borrow_mut_data()?;
        // Note that this memcpy doesn't necessarily overwrite all existing data in the account.
        // This case is handled by updating the .size_ field below.
        sol_memcpy(
            &mut data[size_of::<ProductAccount>()..],
            new_data,
            new_data.len(),
        );
    }

    let mut product_data = load_checked::<ProductAccount>(product_account, hdr.version)?;
    product_data.header.size = try_convert(size_of::<ProductAccount>() + new_data.len())?;

    Ok(())
}
