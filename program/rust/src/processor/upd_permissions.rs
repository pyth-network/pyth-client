use {
    crate::{
        accounts::{
            PermissionAccount,
            PythAccount,
            PERMISSIONS_SEED,
        },
        deserialize::{
            load,
            load_checked,
        },
        instruction::UpdPermissionsArgs,
        utils::{
            check_is_upgrade_authority_for_program,
            check_valid_funding_account,
            check_valid_writable_account,
            pyth_assert,
        },
        OracleError,
    },
    solana_program::{
        account_info::AccountInfo,
        entrypoint::ProgramResult,
        pubkey::Pubkey,
        system_program::check_id,
    },
};

/// Updates permissions for the pyth oracle program
/// This function can create and update the permissions accounts, which stores
/// several public keys that can execute administrative instructions in the pyth program
// key[0] upgrade authority         [signer writable]
// key[1] program account           []
// key[2] programdata account       []
// key[3] permissions account       [writable]
// key[4] system program            []
pub fn upd_permissions(
    program_id: &Pubkey,
    accounts: &[AccountInfo],
    instruction_data: &[u8],
) -> ProgramResult {
    let [funding_account, programdata_account, permissions_account, system_program] = match accounts
    {
        [w, x, y, z] => Ok([w, x, y, z]),
        _ => Err(OracleError::InvalidNumberOfAccounts),
    }?;

    let cmd_args = load::<UpdPermissionsArgs>(instruction_data)?;

    check_valid_funding_account(funding_account)?;
    check_is_upgrade_authority_for_program(funding_account, programdata_account, program_id)?;

    let (permission_pda_address, bump_seed) =
        Pubkey::find_program_address(&[PERMISSIONS_SEED.as_bytes()], program_id);
    pyth_assert(
        permission_pda_address == *permissions_account.key,
        OracleError::InvalidPda.into(),
    )?;

    pyth_assert(
        check_id(system_program.key),
        OracleError::InvalidSystemAccount.into(),
    )?;


    // Create PermissionAccount if it doesn't exist
    PermissionAccount::initialize_pda(
        permissions_account,
        funding_account,
        system_program,
        program_id,
        &[PERMISSIONS_SEED.as_bytes(), &[bump_seed]],
        cmd_args.header.version,
    )?;

    check_valid_writable_account(program_id, permissions_account)?;

    let mut permissions_account_data =
        load_checked::<PermissionAccount>(permissions_account, cmd_args.header.version)?;
    permissions_account_data.master_authority = cmd_args.master_authority;

    permissions_account_data.data_curation_authority = cmd_args.data_curation_authority;
    permissions_account_data.security_authority = cmd_args.security_authority;

    Ok(())
}
