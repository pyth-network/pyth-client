use {
    crate::{
        accounts::PriceAccount,
        c_oracle_header::PRICE_ACCOUNT_SIZE,
        error::OracleError,
        tests::pyth_simulator::PythSimulator,
        utils::try_convert,
    },
    solana_sdk::{
        account::Account,
        signature::Keypair,
        signer::Signer,
    },
    std::mem::size_of,
};

#[tokio::test]
async fn test_resize_price_account() {
    let mut sim = PythSimulator::new().await;
    let publisher = Keypair::new();
    let security_authority = Keypair::new();
    let price_accounts = sim
        .setup_product_fixture(publisher.pubkey(), security_authority.pubkey())
        .await;
    let price = price_accounts["LTC"];

    // Check size after initialization
    let price_account = sim.get_account(price).await.unwrap();
    assert_eq!(price_account.data.len(), size_of::<PriceAccount>());

    // Run with the wrong authority
    assert_eq!(
        sim.resize_price_account(price, &publisher)
            .await
            .unwrap_err()
            .unwrap(),
        OracleError::PermissionViolation.into()
    );

    // Run the instruction once
    assert!(sim
        .resize_price_account(price, &security_authority)
        .await
        .is_ok());
    // Check new size
    let price_account: Account = sim.get_account(price).await.unwrap();
    assert_eq!(
        price_account.data.len(),
        try_convert::<_, usize>(PRICE_ACCOUNT_SIZE).unwrap()
    );

    // Future calls don't change the size
    assert!(sim
        .resize_price_account(price, &security_authority)
        .await
        .is_ok());
    let price_account = sim.get_account(price).await.unwrap();
    assert_eq!(
        price_account.data.len(),
        try_convert::<_, usize>(PRICE_ACCOUNT_SIZE).unwrap()
    );
}
