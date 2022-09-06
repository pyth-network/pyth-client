use solana_sdk::signer::Signer;

use crate::tests::pyth_simulator::PythSimulator;

#[tokio::test]
async fn test_upd_sma2() {
    let mut sim = PythSimulator::new().await;
    let mapping_keypair = sim.init_mapping().await.unwrap();
    let product1 = sim.add_product(&mapping_keypair).await.unwrap();

    let price1 = sim.add_price(&product1, -8).await.unwrap();
    sim.add_publisher(&price1).await.unwrap();
    sim.upd_price(&price1.pubkey()).await.unwrap();
}
