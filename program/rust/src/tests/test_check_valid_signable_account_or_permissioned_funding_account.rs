use {
    super::test_utils::AccountSetup,
    crate::{
        accounts::{
            PermissionAccount,
            ProductAccount,
            PythAccount,
        },
        c_oracle_header::PC_VERSION,
        instruction::{
            CommandHeader,
            OracleCommand,
        },
        utils::check_valid_signable_account_or_permissioned_funding_account,
    },
    solana_program::program_error::ProgramError,
    solana_sdk::signature::{
        Keypair,
        Signer,
    },
};

/// NOTE(2023-11-19): This check was recently modified to throw errors
/// on missing permissions account. This simple test case ensures that
/// a missing permissions account is causing failures unconditionally.
#[test]
pub fn test_permissions_account_mandatory() {
    let program_kp = Keypair::new();
    let program_id = program_kp.pubkey();

    let prod_kp = Keypair::new();
    let mut prod_setup = AccountSetup::new::<ProductAccount>(&prod_kp.pubkey());
    let mut prod_account = prod_setup.as_account_info();

    prod_account.is_writable = true;
    prod_account.is_signer = true;
    prod_account.owner = &program_id;

    let mut funding_setup = AccountSetup::new_funding();
    let mut funding_account = funding_setup.as_account_info();

    funding_account.is_writable = true;
    funding_account.is_signer = true;

    let mut permissions_setup = AccountSetup::new_permission(&program_kp.pubkey());
    let permissions_account = permissions_setup.as_account_info();

    {
        let mut permissions_account_data =
            PermissionAccount::initialize(&permissions_account, PC_VERSION).unwrap();
        permissions_account_data.master_authority = *funding_account.key;
        permissions_account_data.data_curation_authority = *funding_account.key;
        permissions_account_data.security_authority = *funding_account.key;
    }

    // Permissions account not specified, but signer statuses correct
    // for deprecated privkey flow.
    assert_eq!(
        check_valid_signable_account_or_permissioned_funding_account(
            &program_kp.pubkey(),
            &prod_account,
            &funding_account,
            None,
            &CommandHeader {
                version: PC_VERSION,
                command: OracleCommand::UpdProduct as i32,
            },
        ),
        Err(ProgramError::Custom(605))
    );

    // Identical, but permission account is specified
    assert_eq!(
        check_valid_signable_account_or_permissioned_funding_account(
            &program_kp.pubkey(),
            &prod_account,
            &funding_account,
            Some(&permissions_account),
            &CommandHeader {
                version: PC_VERSION,
                command: OracleCommand::UpdProduct as i32,
            },
        ),
        Ok(()),
    );
}
