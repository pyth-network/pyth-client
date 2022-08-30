use solana_program::pubkey::Pubkey;
use solana_sdk::signer::Signer;

use crate::c_oracle_header::PermissionAccount;
use crate::error::OracleError;
use crate::instruction::{
    OracleCommand,
    UpdPermissionsArgs,
};
use crate::tests::pyth_simulator::PythSimulator;
#[tokio::test]
async fn test_upd_permissions() {
    let mut sim = PythSimulator::new().await;

    //Check that the program got deployed properly and can be interacted with
    let mapping_keypair = sim.init_mapping().await.unwrap();
    let product = sim.add_product(&mapping_keypair).await.unwrap();
    let price = sim.add_price(&product, -8).await.unwrap();
    assert!(sim.get_account(price.pubkey()).await.is_some());

    let mut master_authority = Pubkey::new_unique();
    let mut data_curation_authority = Pubkey::new_unique();
    let mut security_authority = Pubkey::new_unique();

    sim.set_generic_as_payer();

    // Should fail because payer is not the authority
    assert_eq!(
        sim.upd_permissions(UpdPermissionsArgs {
            header: OracleCommand::UpdPermissions.into(),
            master_authority,
            data_curation_authority,
            security_authority,
        })
        .await
        .unwrap_err()
        .unwrap(),
        OracleError::FailedAuthenticatingUpgradeAuthority.into()
    );

    sim.set_upgrade_authority_as_payer();

    let mut permissions_pubkey = sim
        .upd_permissions(UpdPermissionsArgs {
            header: OracleCommand::UpdPermissions.into(),
            master_authority,
            data_curation_authority,
            security_authority,
        })
        .await
        .unwrap();
    let mut permission_data = sim
        .get_account_data_as::<PermissionAccount>(permissions_pubkey)
        .await
        .unwrap();

    assert_eq!(master_authority, permission_data.master_authority);
    assert_eq!(
        data_curation_authority,
        permission_data.data_curation_authority
    );
    assert_eq!(security_authority, permission_data.security_authority);
    let initialized_bump = permission_data.bump;
    assert!(initialized_bump != 0);

    data_curation_authority = Pubkey::new_unique();
    master_authority = Pubkey::new_unique();
    security_authority = Pubkey::new_unique();

    sim.set_generic_as_payer();

    // Should fail because payer is not the authority
    assert_eq!(
        sim.upd_permissions(UpdPermissionsArgs {
            header: OracleCommand::UpdPermissions.into(),
            master_authority,
            data_curation_authority,
            security_authority,
        })
        .await
        .unwrap_err()
        .unwrap(),
        OracleError::FailedAuthenticatingUpgradeAuthority.into()
    );

    sim.set_upgrade_authority_as_payer();

    permissions_pubkey = sim
        .upd_permissions(UpdPermissionsArgs {
            header: OracleCommand::UpdPermissions.into(),
            master_authority,
            data_curation_authority,
            security_authority,
        })
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
    assert_eq!(initialized_bump, permission_data.bump);
}
