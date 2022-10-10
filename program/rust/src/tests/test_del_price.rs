use crate::accounts::ProductAccount;

use {
    crate::tests::pyth_simulator::PythSimulator,
    solana_program::pubkey::Pubkey,
    solana_sdk::signer::Signer,
};

#[tokio::test]
async fn test_del_price() {
    let mut sim = PythSimulator::new().await;
    let mapping_keypair = sim.init_mapping().await.unwrap();
    let product1 = sim.add_product(&mapping_keypair).await.unwrap();
    let product2 = sim.add_product(&mapping_keypair).await.unwrap();
    let product3 = sim.add_product(&mapping_keypair).await.unwrap();
    let price1 = sim.add_price(&product1, -8).await.unwrap();
    let price2_1 = sim.add_price(&product2, -8).await.unwrap();
    let price2_2 = sim.add_price(&product2, -8).await.unwrap();

    assert!(sim.get_account(price1.pubkey()).await.is_some());
    assert!(sim.get_account(price2_1.pubkey()).await.is_some());

    assert!(sim.del_price(&product2, &price1).await.is_err());
    assert!(sim.del_price(&product1, &price2_1).await.is_err());
    assert!(sim.del_price(&product1, &price2_2).await.is_err());
    assert!(sim.del_price(&product3, &price2_1).await.is_err());
    assert!(sim.del_price(&product3, &price2_2).await.is_err());

    sim.del_price(&product1, &price1).await.unwrap();
    assert!(sim.get_account(price1.pubkey()).await.is_none());

    let product1_data = sim
        .get_account_data_as::<ProductAccount>(product1.pubkey())
        .await
        .unwrap();
    assert!(product1_data.first_price_account == Pubkey::default());


    // price2_1 is the 2nd item in the linked list since price2_2 got added after t.
    assert!(sim.del_price(&product2, &price2_1).await.is_err());
    // Can delete the accounts in the opposite order though
    assert!(sim.del_price(&product2, &price2_2).await.is_ok());
    assert!(sim.del_price(&product2, &price2_1).await.is_ok());

    assert!(sim.get_account(price2_2.pubkey()).await.is_none());
    assert!(sim.get_account(price2_1.pubkey()).await.is_none());

    let product2_data = sim
        .get_account_data_as::<ProductAccount>(product2.pubkey())
        .await
        .unwrap();

    assert!(product2_data.first_price_account == Pubkey::default());
}
