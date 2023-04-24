use {
    crate::{
        accounts::PriceAccount,
        c_oracle_header::{
            PC_STATUS_TRADING,
            PC_STATUS_UNKNOWN,
            PRICE_ACCOUNT_DEFAULT_MIN_PUB,
        },
        tests::pyth_simulator::{
            PythSimulator,
            Quote,
        },
    },
    solana_program::pubkey::Pubkey,
    solana_sdk::{
        signature::Keypair,
        signer::Signer,
    },
};


#[tokio::test]
async fn test_publish() {
    let mut sim = PythSimulator::new().await;
    let publisher = Keypair::new();
    let price_accounts = sim.setup_product_fixture(publisher.pubkey()).await;
    let price = price_accounts["LTC"];

    // Check price account before publishing
    {
        let price_data = sim
            .get_account_data_as::<PriceAccount>(price)
            .await
            .unwrap();

        assert_eq!(price_data.exponent, -5);
        assert_eq!(price_data.price_type, 1);
        assert_eq!(price_data.min_pub_, PRICE_ACCOUNT_DEFAULT_MIN_PUB);
        assert_eq!(price_data.next_price_account, Pubkey::default());

        assert_eq!(price_data.agg_.status_, PC_STATUS_UNKNOWN);
        assert_eq!(price_data.agg_.price_, 0);
        assert_eq!(price_data.agg_.conf_, 0);

        assert_eq!(price_data.num_, 1);
        assert_eq!(price_data.comp_[0].pub_, publisher.pubkey());

        assert_eq!(price_data.comp_[0].latest_.price_, 0);
        assert_eq!(price_data.comp_[0].latest_.conf_, 0);
        assert_eq!(price_data.comp_[0].latest_.status_, PC_STATUS_UNKNOWN);
    }

    sim.upd_price(
        &publisher,
        price,
        Quote {
            price:      150,
            confidence: 7,
            status:     PC_STATUS_TRADING,
        },
    )
    .await
    .unwrap();

    {
        let price_data = sim
            .get_account_data_as::<PriceAccount>(price)
            .await
            .unwrap();

        assert_eq!(price_data.comp_[0].latest_.price_, 150);
        assert_eq!(price_data.comp_[0].latest_.conf_, 7);
        assert_eq!(price_data.comp_[0].latest_.status_, PC_STATUS_TRADING);

        assert_eq!(price_data.comp_[0].agg_.price_, 0);
        assert_eq!(price_data.comp_[0].agg_.conf_, 0);
        assert_eq!(price_data.comp_[0].agg_.status_, PC_STATUS_UNKNOWN);
    }

    sim.warp_to_slot(2).await.unwrap();
    sim.upd_price(
        &publisher,
        price,
        Quote {
            price:      0,
            confidence: 0,
            status:     PC_STATUS_UNKNOWN,
        },
    )
    .await
    .unwrap();


    {
        let price_data = sim
            .get_account_data_as::<PriceAccount>(price)
            .await
            .unwrap();

        assert_eq!(price_data.comp_[0].latest_.price_, 0);
        assert_eq!(price_data.comp_[0].latest_.conf_, 0);
        assert_eq!(price_data.comp_[0].latest_.status_, PC_STATUS_UNKNOWN);

        assert_eq!(price_data.comp_[0].agg_.price_, 150);
        assert_eq!(price_data.comp_[0].agg_.conf_, 7);
        assert_eq!(price_data.comp_[0].agg_.status_, PC_STATUS_TRADING);
    }

    sim.warp_to_slot(3).await.unwrap();
    sim.upd_price(
        &publisher,
        price,
        Quote {
            price:      100,
            confidence: 1,
            status:     PC_STATUS_TRADING,
        },
    )
    .await
    .unwrap();

    {
        let price_data = sim
            .get_account_data_as::<PriceAccount>(price)
            .await
            .unwrap();

        assert_eq!(price_data.comp_[0].latest_.price_, 100);
        assert_eq!(price_data.comp_[0].latest_.conf_, 1);
        assert_eq!(price_data.comp_[0].latest_.status_, PC_STATUS_TRADING);

        assert_eq!(price_data.comp_[0].agg_.price_, 0);
        assert_eq!(price_data.comp_[0].agg_.conf_, 0);
        assert_eq!(price_data.comp_[0].agg_.status_, PC_STATUS_UNKNOWN);
    }

    sim.warp_to_slot(4).await.unwrap();
    sim.upd_price(
        &publisher,
        price,
        Quote {
            price:      0,
            confidence: 0,
            status:     PC_STATUS_UNKNOWN,
        },
    )
    .await
    .unwrap();

    {
        let price_data = sim
            .get_account_data_as::<PriceAccount>(price)
            .await
            .unwrap();

        assert_eq!(price_data.comp_[0].latest_.price_, 0);
        assert_eq!(price_data.comp_[0].latest_.conf_, 0);
        assert_eq!(price_data.comp_[0].latest_.status_, PC_STATUS_UNKNOWN);

        assert_eq!(price_data.comp_[0].agg_.price_, 100);
        assert_eq!(price_data.comp_[0].agg_.conf_, 1);
        assert_eq!(price_data.comp_[0].agg_.status_, PC_STATUS_TRADING);
    }
}
