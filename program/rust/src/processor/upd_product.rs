use {
    crate::{
        accounts::{
            update_product_metadata,
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
        },
        OracleError,
    },
    solana_program::{
        account_info::AccountInfo,
        entrypoint::ProgramResult,
        pubkey::Pubkey,
    },
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
    let (funding_account, product_account, permissions_account) = match accounts {
        [x, y, p] => Ok((x, y, p)),
        _ => Err(OracleError::InvalidNumberOfAccounts),
    }?;

    let hdr = load::<CommandHeader>(instruction_data)?;

    check_valid_funding_account(funding_account)?;
    check_permissioned_funding_account(
        program_id,
        product_account,
        funding_account,
        permissions_account,
        hdr,
    )?;


    {
        // Validate that product_account contains the appropriate account header
        let mut _product_data = load_checked::<ProductAccount>(product_account, hdr.version)?;
    }

    update_product_metadata(instruction_data, product_account, hdr.version)?;

    Ok(())
}
