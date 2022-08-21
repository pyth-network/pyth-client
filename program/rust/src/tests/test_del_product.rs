use solana_sdk::signer::Signer;

use crate::c_oracle_header::pc_map_table_t;
use crate::tests::pyth_simulator::PythSimulator;
use crate::utils::{
    pubkey_equal,
    pubkey_is_zero,
};

#[tokio::test]
async fn test_del_product() {
    let mut sim = PythSimulator::new().await;
    let mapping_keypair = sim.init_mapping().await.unwrap();
    let _product1 = sim.add_product(&mapping_keypair).await.unwrap();
    let product2 = sim.add_product(&mapping_keypair).await.unwrap();
    let product3 = sim.add_product(&mapping_keypair).await.unwrap();
    let product4 = sim.add_product(&mapping_keypair).await.unwrap();
    let product5 = sim.add_product(&mapping_keypair).await.unwrap();
    let _price3 = sim.add_price(&product3, -8).await.unwrap();

    let mapping_keypair2 = sim.init_mapping().await.unwrap();
    let product1_2 = sim.add_product(&mapping_keypair2).await.unwrap();

    assert!(sim.get_account(product2.pubkey()).await.is_some());
    assert!(sim.get_account(product4.pubkey()).await.is_some());

    // Can't delete a product with a price account
    assert!(sim.del_product(&mapping_keypair, &product3).await.is_err());
    // Can't delete mismatched product/mapping accounts
    assert!(sim
        .del_product(&mapping_keypair, &product1_2)
        .await
        .is_err());

    assert!(sim.del_product(&mapping_keypair, &product2).await.is_ok());

    let mapping_data = sim
        .get_account_data_as::<pc_map_table_t>(mapping_keypair.pubkey())
        .await
        .unwrap();
    assert_eq!(mapping_data.num_, 4);

    assert!(pubkey_equal(
        &mapping_data.prod_[1],
        &product5.pubkey().to_bytes()
    ));
    assert!(pubkey_equal(
        &mapping_data.prod_[3],
        &product4.pubkey().to_bytes()
    ));
    assert!(pubkey_is_zero(&mapping_data.prod_[4]));
}
