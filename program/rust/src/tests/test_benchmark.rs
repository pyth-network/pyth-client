use {
    crate::{
        c_oracle_header::PC_STATUS_TRADING,
        tests::pyth_simulator::{
            PythSimulator,
            Quote,
        },
    },
    rand::{
        Rng,
        SeedableRng,
    },
    solana_program::native_token::LAMPORTS_PER_SOL,
    solana_sdk::{
        signature::Keypair,
        signer::Signer,
    },
};

/// Benchmark the execution of the oracle program
async fn run_benchmark(num_publishers: usize) -> Result<(), Box<dyn std::error::Error>> {
    let mut sim = PythSimulator::new().await;

    let mapping_keypair = sim.init_mapping().await?;
    let product_keypair = sim.add_product(&mapping_keypair).await?;
    let price_keypair = sim.add_price(&product_keypair, -5).await?;
    let price_pubkey = price_keypair.pubkey();

    let mut publishers_keypairs: Vec<_> = (0..num_publishers).map(|_idx| Keypair::new()).collect();
    publishers_keypairs.sort_by_key(|kp| kp.pubkey());

    let publishers_pubkeys: Vec<_> = publishers_keypairs.iter().map(|kp| kp.pubkey()).collect();

    for publisher in publishers_pubkeys.iter() {
        sim.airdrop(publisher, 100 * LAMPORTS_PER_SOL).await?;
        sim.add_publisher(&price_keypair, *publisher).await?;
    }

    // Set the seed to make the test deterministic
    let mut rnd = rand::rngs::SmallRng::seed_from_u64(14);

    for kp in publishers_keypairs.iter() {
        // The ranges are chosen to create overlap between
        // publishers price set (price-conf, price, price + conf)
        let quote = Quote {
            price:      rnd.gen_range(10000..11000),
            confidence: rnd.gen_range(1..1000),
            status:     PC_STATUS_TRADING,
        };

        sim.upd_price(kp, price_pubkey, quote).await?;
    }

    // Advance slot once from 1 to 2
    sim.warp_to_slot(2).await?;

    // Final price update to trigger aggregation
    let first_kp = publishers_keypairs.first().unwrap();
    let first_quote = Quote {
        price:      100,
        confidence: 30,
        status:     PC_STATUS_TRADING,
    };
    sim.upd_price(first_kp, price_pubkey, first_quote).await?;

    Ok(())
}

#[tokio::test]
async fn test_benchmark_64_pubs() -> Result<(), Box<dyn std::error::Error>> {
    run_benchmark(64).await
}

#[tokio::test]
async fn test_benchmark_32_pubs() -> Result<(), Box<dyn std::error::Error>> {
    run_benchmark(32).await
}

#[tokio::test]
async fn test_benchmark_16_pubs() -> Result<(), Box<dyn std::error::Error>> {
    run_benchmark(16).await
}

#[tokio::test]
async fn test_benchmark_8_pubs() -> Result<(), Box<dyn std::error::Error>> {
    run_benchmark(8).await
}
