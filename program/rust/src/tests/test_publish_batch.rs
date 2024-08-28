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
        utils::get_status_for_conf_price_ratio,
    },
    solana_program::pubkey::Pubkey,
    solana_sdk::{
        signature::Keypair,
        signer::Signer,
    },
    std::collections::HashMap,
};

#[tokio::test]
async fn test_publish_batch() {
    let mut sim = PythSimulator::new().await;
    let publisher = Keypair::new();
    let security_authority = Keypair::new();
    let price_accounts = sim
        .setup_product_fixture(&[publisher.pubkey()], security_authority.pubkey())
        .await;

    for price in price_accounts.values() {
        let price_data = sim
            .get_account_data_as::<PriceAccount>(*price)
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

        assert_eq!(price_data.comp_[0].agg_.price_, 0);
        assert_eq!(price_data.comp_[0].agg_.conf_, 0);
        assert_eq!(price_data.comp_[0].agg_.status_, PC_STATUS_UNKNOWN);
    }

    let mut quotes: HashMap<String, Quote> = HashMap::new();
    for key in price_accounts.keys() {
        quotes.insert(
            key.to_string(),
            Quote {
                price:      rand::random::<i64>() % 150 + 1,
                confidence: rand::random::<u64>() % 20 + 1,
                status:     PC_STATUS_TRADING,
            },
        );
    }

    sim.upd_price_batch(&publisher, &price_accounts, &quotes)
        .await
        .unwrap();

    for (key, price) in &price_accounts {
        let price_data = sim
            .get_account_data_as::<PriceAccount>(*price)
            .await
            .unwrap();
        let quote = quotes.get(key).unwrap();
        assert_eq!(price_data.comp_[0].latest_.price_, quote.price);
        assert_eq!(price_data.comp_[0].latest_.conf_, quote.confidence);
        assert_eq!(
            price_data.comp_[0].latest_.status_,
            get_status_for_conf_price_ratio(quote.price, quote.confidence, quote.status).unwrap()
        );
        assert_eq!(price_data.comp_[0].agg_.price_, 0);
        assert_eq!(price_data.comp_[0].agg_.conf_, 0);
        assert_eq!(price_data.comp_[0].agg_.status_, PC_STATUS_UNKNOWN);
    }

    sim.warp_to_slot(2).await.unwrap();

    let mut new_quotes: HashMap<String, Quote> = HashMap::new();
    for key in price_accounts.keys() {
        new_quotes.insert(
            key.to_string(),
            Quote {
                price:      rand::random::<i64>() % 150 + 1,
                confidence: rand::random::<u64>() % 20 + 1,
                status:     PC_STATUS_TRADING,
            },
        );
    }
    sim.upd_price_batch(&publisher, &price_accounts, &new_quotes)
        .await
        .unwrap();

    for (key, price) in &price_accounts {
        let price_data = sim
            .get_account_data_as::<PriceAccount>(*price)
            .await
            .unwrap();
        let _quote = quotes.get(key).unwrap();
        let new_quote = new_quotes.get(key).unwrap();

        assert_eq!(price_data.comp_[0].latest_.price_, new_quote.price);
        assert_eq!(price_data.comp_[0].latest_.conf_, new_quote.confidence);
        assert_eq!(
            price_data.comp_[0].latest_.status_,
            get_status_for_conf_price_ratio(
                new_quote.price,
                new_quote.confidence,
                new_quote.status
            )
            .unwrap()
        );
        assert_eq!(price_data.comp_[0].agg_.price_, 0);
        assert_eq!(price_data.comp_[0].agg_.conf_, 0);
        assert_eq!(price_data.comp_[0].agg_.status_, PC_STATUS_UNKNOWN);
    }
}
