use std::str::FromStr;

use bytemuck::bytes_of;
use solana_program::bpf_loader_upgradeable::UpgradeableLoaderState;
use solana_program::pubkey::Pubkey;
use solana_sdk::signer::Signer;

use crate::c_oracle_header::{
    CPubkey,
    PermissionAccount,
    ProductAccount,
};
use crate::instruction::{
    OracleCommand,
    UpdPermissionsArgs,
};
use crate::tests::pyth_simulator::PythSimulator;
use crate::utils::{
    pubkey_equal,
    pubkey_is_zero,
};

#[tokio::test]
async fn test_upd_permissions() {
    let mut sim = PythSimulator::new_upgradable().await;

    //Check that the program got deployed properly
    let mapping_keypair = sim.init_mapping().await.unwrap();
    let product = sim.add_product(&mapping_keypair).await.unwrap();
    let price = sim.add_price(&product, -8).await.unwrap();
    assert!(sim.get_account(price.pubkey()).await.is_some());

    let mut data_curation_authority = CPubkey::new_unique();
    let mut master_authority = CPubkey::new_unique();
    let mut security_authority = CPubkey::new_unique();

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

    assert!(pubkey_equal(
        &master_authority,
        bytes_of(&permission_data.master_authority)
    ));
    assert!(pubkey_equal(
        &data_curation_authority,
        bytes_of(&permission_data.data_curation_authority)
    ));
    assert!(pubkey_equal(
        &security_authority,
        bytes_of(&permission_data.security_authority)
    ));

    data_curation_authority = CPubkey::new_unique();
    master_authority = CPubkey::new_unique();
    security_authority = CPubkey::new_unique();

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

    assert!(pubkey_equal(
        &master_authority,
        bytes_of(&permission_data.master_authority)
    ));
    assert!(pubkey_equal(
        &data_curation_authority,
        bytes_of(&permission_data.data_curation_authority)
    ));
    assert!(pubkey_equal(
        &security_authority,
        bytes_of(&permission_data.security_authority)
    ));
}