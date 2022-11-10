use {
    crate::{
        c_oracle_header::{
            PermissionAccount,
            PythAccount,
        },
        deserialize::load,
        error::OracleError,
        instruction::{
            OracleCommand,
            UpdPermissionsArgs,
        },
        tests::pyth_simulator::{
            copy_keypair,
            PythSimulator,
        },
    },
    solana_program::{
        pubkey::Pubkey,
        rent::Rent,
    },
    solana_sdk::signer::Signer,
};

#[tokio::test]
async fn test_upd_permissions() {
    let mut sim = PythSimulator::new().await;

    // Check that the program got deployed properly and can be interacted with
    let mapping_keypair = sim.init_mapping().await.unwrap();
    let product = sim.add_product(&mapping_keypair).await.unwrap();
    let price = sim.add_price(&product, -8).await.unwrap();
    assert!(sim.get_account(price.pubkey()).await.is_some());

    let mut master_authority = Pubkey::new_unique();
    let mut data_curation_authority = Pubkey::new_unique();
    let mut security_authority = Pubkey::new_unique();

    // Airdrop some lamports to check whether someone can DDOS the account
    let permissions_pubkey = sim.get_permissions_pubkey();
    sim.airdrop(&permissions_pubkey, Rent::default().minimum_balance(0))
        .await
        .unwrap();
    let permission_account = sim.get_account(permissions_pubkey).await.unwrap();
    assert_eq!(
        permission_account.lamports,
        Rent::default().minimum_balance(0)
    );

    // Should fail because payer is not the authority
    assert_eq!(
        sim.upd_permissions(
            UpdPermissionsArgs {
                header: OracleCommand::UpdPermissions.into(),
                master_authority,
                data_curation_authority,
                security_authority,
            },
            &copy_keypair(&sim.genesis_keypair)
        )
        .await
        .unwrap_err()
        .unwrap(),
        OracleError::InvalidUpgradeAuthority.into()
    );

    let mut permissions_pubkey = sim
        .upd_permissions(
            UpdPermissionsArgs {
                header: OracleCommand::UpdPermissions.into(),
                master_authority,
                data_curation_authority,
                security_authority,
            },
            &copy_keypair(&sim.upgrade_authority),
        )
        .await
        .unwrap();
    let permission_account = sim.get_account(permissions_pubkey).await.unwrap();

    assert_eq!(
        permission_account.data.len(),
        PermissionAccount::MINIMUM_SIZE
    );
    assert_eq!(
        Rent::default().minimum_balance(permission_account.data.len()),
        permission_account.lamports
    );
    assert!(sim.is_owned_by_oracle(&permission_account));

    let mut permission_data =
        *load::<PermissionAccount>(permission_account.data.as_slice()).unwrap();

    assert_eq!(master_authority, permission_data.master_authority);
    assert_eq!(
        data_curation_authority,
        permission_data.data_curation_authority
    );
    assert_eq!(security_authority, permission_data.security_authority);

    data_curation_authority = Pubkey::new_unique();
    master_authority = Pubkey::new_unique();
    security_authority = Pubkey::new_unique();


    // Should fail because payer is not the authority
    assert_eq!(
        sim.upd_permissions(
            UpdPermissionsArgs {
                header: OracleCommand::UpdPermissions.into(),
                master_authority,
                data_curation_authority,
                security_authority,
            },
            &copy_keypair(&sim.genesis_keypair)
        )
        .await
        .unwrap_err()
        .unwrap(),
        OracleError::InvalidUpgradeAuthority.into()
    );
    permissions_pubkey = sim
        .upd_permissions(
            UpdPermissionsArgs {
                header: OracleCommand::UpdPermissions.into(),
                master_authority,
                data_curation_authority,
                security_authority,
            },
            &copy_keypair(&sim.upgrade_authority),
        )
        .await
        .unwrap();
    permission_data = sim
        .get_account_data_as::<PermissionAccount>(permissions_pubkey)
        .await
        .unwrap();

    assert_eq!(master_authority, permission_data.master_authority);
    assert_eq!(
        data_curation_authority,
        permission_data.data_curation_authority
    );
    assert_eq!(security_authority, permission_data.security_authority);
}
