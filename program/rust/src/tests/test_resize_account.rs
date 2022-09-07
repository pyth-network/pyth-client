use solana_sdk::signer::Signer;
use std::mem::size_of;

use crate::c_oracle_header::pc_price_t;
use crate::tests::pyth_simulator::PythSimulator;
use crate::time_machine_types::PriceAccountWrapper;


/// Warning : This test will fail if you run cargo test instead of cargo test-bpf
#[tokio::test]
async fn test_resize_account() {
    let mut sim = PythSimulator::new().await;
    let mapping_keypair = sim.init_mapping().await.unwrap();
    let product1 = sim.add_product(&mapping_keypair).await.unwrap();
    let price1 = sim.add_price(&product1, -8).await.unwrap();

    // Check size after initialization
    let price1_account = sim.get_account(price1.pubkey()).await.unwrap();
    assert_eq!(price1_account.data.len(), size_of::<pc_price_t>());

    // Run the instruction once
    assert!(sim.resize_price_account(&price1).await.is_ok());
    // Check new size
    let price1_account = sim.get_account(price1.pubkey()).await.unwrap();
    assert_eq!(price1_account.data.len(), size_of::<PriceAccountWrapper>());

    // Future calls don't change the size
    assert!(sim.resize_price_account(&price1).await.is_ok());
    let price1_account = sim.get_account(price1.pubkey()).await.unwrap();
    assert_eq!(price1_account.data.len(), size_of::<PriceAccountWrapper>());
    let price1_account_data = sim
        .get_account_data_as::<PriceAccountWrapper>(price1.pubkey())
        .await
        .unwrap();
    assert_eq!(price1_account_data.time_machine.granularity, 0);
    assert_eq!(price1_account_data.time_machine.threshold, 0);
}
