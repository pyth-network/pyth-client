use {
    crate::{
        accounts::PriceAccount,
        c_oracle_header::{
            PC_NUM_COMP,
            PC_STATUS_TRADING,
        }, //PC_STATUS_UNKNOWN},
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

    // Divide publishers into three ~even parts +/- 1 pub
    let (first_third, other_two_thirds) = pub_keypairs.split_at(n_pubs / 3 + 1);
    let (mid_third, last_third) = other_two_thirds.split_at(n_pubs / 3 + 1);

    #[cfg(feature = "pythnet")]
    let n_repeats = 1;

    // On solana, integer 2/3 of 32 is 20, but it is the 21st pub that
    // will update aggregate under default min_pub. At the same time,
    // upd_price updates a publisher's price *after* aggregate, which
    // means that the min_pub-th update will *not* update aggregate
    // (min_pub-th + 1 will). We repeat the first/mid iteration to
    // ensure that the aggregate is also updated on Solana.
    #[cfg(not(feature = "pythnet"))]
    let n_repeats = 2;

    // for _ in 0..n_repeats {
    //     for (first_kp, mid_kp) in first_third.iter().zip(mid_third.iter()) {
    //         let first_quote = Quote {
    //             price: 100,
    //             confidence: 30,
    //             status: PC_STATUS_TRADING,
    //         };

    //         sim.upd_price(first_kp, price, first_quote).await?;

    //         let mid_quote = Quote {
    //             price: 120,
    //             confidence: 30,
    //             status: PC_STATUS_TRADING,
    //         };

    //         sim.upd_price(mid_kp, price, mid_quote).await?;

    //         slot += 1;
    //         sim.warp_to_slot(slot).await?;
    //     }
    // }

    // {
    //     let price_data = sim
    //         .get_account_data_as::<PriceAccount>(price)
    //         .await
    //         .unwrap();

    //     assert_eq!(price_data.agg_.price_, 110);
    //     assert_eq!(price_data.agg_.conf_, 20);
    // }

    // // Exclude first third from aggregation
    // for first_kp in first_third {
    //     let first_quote = Quote {
    //         price: 42,
    //         confidence: 1,
    //         status: PC_STATUS_UNKNOWN,
    //     };

    //     sim.upd_price(first_kp, price, first_quote).await?;

    //     slot += 1;
    //     sim.warp_to_slot(slot).await?;
    // }


    dbg!(first_third.len());
    dbg!(mid_third.len());
    dbg!(last_third.len());

    drop(first_third);
    for i in 0..n_repeats {
        eprintln!("Pass {}", i);
        for (mid_kp, last_kp) in mid_third.iter().zip(last_third.iter()) {
            let mid_quote = Quote {
                price:      60,
                confidence: 30,
                status:     PC_STATUS_TRADING,
            };

            sim.upd_price(mid_kp, price, mid_quote).await?;

            let last_quote = Quote {
                price:      80,
                confidence: 30,
                status:     PC_STATUS_TRADING,
            };

            sim.upd_price(last_kp, price, last_quote).await?;

            slot += 1;
            // sim.warp_to_slot(slot).await?;
        }
    }

    {
        let price_data = sim
            .get_account_data_as::<PriceAccount>(price)
            .await
            .unwrap();

        assert_eq!(price_data.agg_.price_, 70);
    }

    Ok(())
}
