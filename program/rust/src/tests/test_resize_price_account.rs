use {
    crate::{
        accounts::PriceAccount,
        c_oracle_header::PRICE_ACCOUNT_SIZE,
        tests::pyth_simulator::PythSimulator,
        utils::try_convert,
    },
    solana_sdk::signer::Signer,
    std::mem::size_of,
};


/// Warning : This test will fail if you run cargo test instead of cargo test-bpf
#[tokio::test]
async fn test_resize_account() {
    let mut sim = PythSimulator::new().await;
    let mapping_keypair = sim.init_mapping().await.unwrap();
    let product = sim.add_product(&mapping_keypair).await.unwrap();
    let price = sim.add_price(&product, -8).await.unwrap();

    // Check size after initialization
    let price_account = sim.get_account(price.pubkey()).await.unwrap();
    assert_eq!(price_account.data.len(), size_of::<PriceAccount>());

    // Run the instruction once
    assert!(sim.resize_price_account(&price).await.is_ok());
    // Check new size
    let price_account: solana_sdk::account::Account =
        sim.get_account(price.pubkey()).await.unwrap();
    assert_eq!(
        price_account.data.len(),
        try_convert::<_, usize>(PRICE_ACCOUNT_SIZE).unwrap()
    );

    // Future calls don't change the size
    assert!(sim.resize_price_account(&price).await.is_ok());
    let price_account = sim.get_account(price.pubkey()).await.unwrap();
    assert_eq!(
        price_account.data.len(),
        try_convert::<_, usize>(PRICE_ACCOUNT_SIZE).unwrap()
    );
}
