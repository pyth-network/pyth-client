use {
    crate::{
        accounts::PriceAccount,
        c_oracle_header::{
            PC_NUM_COMP,
            PC_STATUS_TRADING,
            PC_STATUS_UNKNOWN,
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
async fn test_full_publisher_set() -> Result<(), Box<dyn std::error::Error>> {
    let mut sim = PythSimulator::new().await;
    let pub_keypairs: Vec<_> = (0..PC_NUM_COMP).map(|_idx| Keypair::new()).collect();
    let pub_pubkeys: Vec<_> = pub_keypairs.iter().map(|kp| kp.pubkey()).collect();

    let security_authority = Keypair::new();
    let price_accounts = sim
        .setup_product_fixture(pub_pubkeys.as_slice(), security_authority.pubkey())
        .await;
    let price = price_accounts["LTC"];

    let mut slot = 1;

    let n_pubs = pub_keypairs.len();

    // Divide publishers into three ~even parts +/- 1 pub. The '+ 1'
    // ensures that for Solana oracle, any two of the parts form a
    // subset that's larger than default min_pub_ value (currently
    // 20).
    let (first_third, other_two_thirds) = pub_keypairs.split_at(n_pubs / 3 + 1);
    let (mid_third, last_third) = other_two_thirds.split_at(n_pubs / 3 + 1);

    for (first_kp, mid_kp) in first_third.iter().zip(mid_third.iter()) {
        let first_quote = Quote {
            price:      100,
            confidence: 30,
            status:     PC_STATUS_TRADING,
        };

        sim.upd_price(first_kp, price, first_quote).await?;

        let mid_quote = Quote {
            price:      120,
            confidence: 30,
            status:     PC_STATUS_TRADING,
        };

        sim.upd_price(mid_kp, price, mid_quote).await?;

        slot += 1;
        sim.warp_to_slot(slot).await?;
    }

    {
        let price_data = sim
            .get_account_data_as::<PriceAccount>(price)
            .await
            .unwrap();

        assert_eq!(price_data.agg_.price_, 110);
        assert_eq!(price_data.agg_.conf_, 20);
    }

    // Exclude first third from aggregation
    for first_kp in first_third {
        let first_quote = Quote {
            price:      42,
            confidence: 1,
            status:     PC_STATUS_UNKNOWN,
        };

        sim.upd_price(first_kp, price, first_quote).await?;

        slot += 1;
        sim.warp_to_slot(slot).await?;
    }

    for (mid_kp, last_kp) in mid_third.iter().zip(last_third.iter()) {
        let mid_quote = Quote {
            price:      60,
            confidence: 10,
            status:     PC_STATUS_TRADING,
        };

        sim.upd_price(mid_kp, price, mid_quote).await?;

        let last_quote = Quote {
            price:      80,
            confidence: 10,
            status:     PC_STATUS_TRADING,
        };

        sim.upd_price(last_kp, price, last_quote).await?;

        slot += 1;
        sim.warp_to_slot(slot).await?;
    }

    {
        let price_data = sim
            .get_account_data_as::<PriceAccount>(price)
            .await
            .unwrap();

        assert_eq!(price_data.agg_.price_, 70);
        assert_eq!(price_data.agg_.conf_, 10);
    }

    Ok(())
}
