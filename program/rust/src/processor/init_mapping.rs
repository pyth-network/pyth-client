use {
    crate::{
        accounts::{
            MappingAccount,
            PythAccount,
        },
        deserialize::load,
        instruction::CommandHeader,
        utils::{
            check_valid_funding_account,
            check_valid_signable_account_or_permissioned_funding_account,
        },
        OracleError,
    },
    solana_program::{
        account_info::AccountInfo,
        entrypoint::ProgramResult,
        pubkey::Pubkey,
    },
};

/// Initialize first mapping list account
// account[0] funding account       [signer writable]
// account[1] mapping account       [signer writable]
pub fn init_mapping(
    program_id: &Pubkey,
    accounts: &[AccountInfo],
    instruction_data: &[u8],
) -> ProgramResult {
    let (funding_account, fresh_mapping_account, permissions_account_option) = match accounts {
        [x, y] => Ok((x, y, None)),
        [x, y, p] => Ok((x, y, Some(p))),
        _ => Err(OracleError::InvalidNumberOfAccounts),
    }?;

    let hdr = load::<CommandHeader>(instruction_data)?;

    check_valid_funding_account(funding_account)?;
    check_valid_signable_account_or_permissioned_funding_account(
        program_id,
        fresh_mapping_account,
        funding_account,
        permissions_account_option,
        hdr,
    )?;

    // Initialize by setting to zero again (just in case) and populating the account header
    MappingAccount::initialize(fresh_mapping_account, hdr.version)?;

    Ok(())
}
