use std::mem::{
    size_of,
    size_of_val,
};

use solana_program::pubkey::Pubkey;
use solana_sdk::signer::Signer;

use crate::c_oracle_header::{
    pc_map_table_t,
    pc_pub_key_t,
};
use crate::tests::pyth_simulator::PythSimulator;
use crate::utils::pubkey_equal;

#[tokio::test]
async fn test_del_product() {
    let mut sim = PythSimulator::new().await;
    let mapping_keypair = sim.init_mapping().await.unwrap();
    let product1 = sim.add_product(&mapping_keypair).await.unwrap();
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

    assert!(sim.get_account(product2.pubkey()).await.is_none());

    let mapping_data = sim
        .get_account_data_as::<pc_map_table_t>(mapping_keypair.pubkey())
        .await
        .unwrap();
    assert!(mapping_product_list_equals(
        &mapping_data,
        vec![
            product1.pubkey(),
            product5.pubkey(),
            product3.pubkey(),
            product4.pubkey()
        ]
    ));
    assert!(sim.get_account(product5.pubkey()).await.is_some());


    assert!(sim.del_product(&mapping_keypair, &product4).await.is_ok());
    let mapping_data = sim
        .get_account_data_as::<pc_map_table_t>(mapping_keypair.pubkey())
        .await
        .unwrap();

    assert!(mapping_product_list_equals(
        &mapping_data,
        vec![product1.pubkey(), product5.pubkey(), product3.pubkey()]
    ));
}

/// Returns true if the list of products in `mapping_data` contains the keys in `expected` (in the
/// same order). Also checks `mapping_data.num_` and `size_`.
fn mapping_product_list_equals(mapping_data: &pc_map_table_t, expected: Vec<Pubkey>) -> bool {
    if mapping_data.num_ != expected.len() as u32 {
        return false;
    }

    let expected_size = (size_of::<pc_map_table_t>() - size_of_val(&mapping_data.prod_)
        + expected.len() * size_of::<pc_pub_key_t>()) as u32;
    if mapping_data.size_ != expected_size {
        return false;
    }

    for i in 0..expected.len() {
        if !pubkey_equal(&mapping_data.prod_[i], &expected[i].to_bytes()) {
            return false;
        }
    }

    return true;
}
