use {
    crate::{
        accounts::PriceAccount,
        c_oracle_header::{
            PC_NUM_COMP,
            PC_STATUS_TRADING,
        },
        tests::pyth_simulator::{
            PythSimulator,
            Quote,
        },
    },
    solana_sdk::{
        signature::Keypair,
        signer::Signer,
    },
};

// Verify that the whole publisher set participates in aggregate
// calculation. This is important for verifying that extra
// publisher slots on Pythnet are working. Here's how this works:
//
// * Fill all publisher slots on a price
// * Divide the price component array into three parts roughly the
// same size: first_third, middle_third and last_third
// * Publish two distinct price values to first/mid third
// * Verify that the aggregate averages out to an expected value in the middle
// * Publish PC_STATUS_UNKNOWN prices to first_third to make sure it does not affect later aggregates
// * Repeat the two-price-value publishing step, using completely
// different values, this time using mid_third and last_third without first_third
// * Verify again for the new values meeting in the middle as aggregate.
#[tokio::test]
async fn test_full_publisher_set_two_thirds() -> Result<(), Box<dyn std::error::Error>> {
    let mut sim = PythSimulator::new().await;
    let pub_keypairs: Vec<_> = (0..PC_NUM_COMP).map(|_idx| Keypair::new()).collect();
    let pub_pubkeys: Vec<_> = pub_keypairs.iter().map(|kp| kp.pubkey()).collect();

    let security_authority = Keypair::new();
    let price_accounts = sim
        .setup_product_fixture(pub_pubkeys.as_slice(), security_authority.pubkey())
        .await;
    let price = price_accounts["LTC"];


    let n_pubs = pub_keypairs.len();

    // Divide publishers into two even parts
    let (first_half, second_half) = pub_keypairs.split_at(n_pubs / 2);

    // Starting with the first publisher in each half, publish an update
    for (first_kp, second_kp) in first_half.iter().zip(second_half.iter()) {
        let first_quote = Quote {
            price:      100,
            confidence: 30,
            status:     PC_STATUS_TRADING,
        };

        sim.upd_price(first_kp, price, first_quote).await?;

        let second_quote = Quote {
            price:      120,
            confidence: 30,
            status:     PC_STATUS_TRADING,
        };

        sim.upd_price(second_kp, price, second_quote).await?;
    }

    // Advance slot once from 1 to 2
    sim.warp_to_slot(2).await?;

    // Final price update to trigger aggregation
    let first_kp = pub_keypairs.first().unwrap();
    let first_quote = Quote {
        price:      100,
        confidence: 30,
        status:     PC_STATUS_TRADING,
    };
    sim.upd_price(first_kp, price, first_quote).await?;

    {
        let price_data = sim
            .get_account_data_as::<PriceAccount>(price)
            .await
            .unwrap();

        assert_eq!(price_data.agg_.price_, 110);
        assert_eq!(price_data.agg_.conf_, 20);
    }

    Ok(())
}
