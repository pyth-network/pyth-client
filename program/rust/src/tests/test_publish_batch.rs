use std::collections::HashMap;

use solana_program::blake3::Hash;
use solana_program::pubkey::Pubkey;
use solana_sdk::{signature::Keypair, signer::Signer};
use crate::c_oracle_header::PC_VERSION;
use crate::c_oracle_header::PC_STATUS_UNKNOWN;
use crate::c_oracle_header::PC_STATUS_TRADING;
use crate::c_oracle_header::PRICE_ACCOUNT_DEFAULT_MIN_PUB;
use crate::tests::pyth_simulator::Quote;
use crate::utils::check_confidence_too_big;
use crate::{tests::pyth_simulator::PythSimulator, accounts::PriceAccount};

#[tokio::test]
async fn test_publish_batch() {
    let mut sim = PythSimulator::new().await;
    let publisher = Keypair::new();
    let price_accounts = sim.setup_product_fixture(publisher.pubkey()).await;
    
    for (key, price) in &price_accounts {
        let price_data = sim.get_account_data_as::<PriceAccount>(*price).await.unwrap();
    
        assert_eq!(price_data.exponent, -5);
        assert_eq!(price_data.price_type, 1);
        assert_eq!(price_data.min_pub_, PRICE_ACCOUNT_DEFAULT_MIN_PUB);
    
        assert_eq!(price_data.agg_.status_, PC_STATUS_UNKNOWN);
        assert_eq!(price_data.agg_.price_, 0);
        assert_eq!(price_data.agg_.conf_, 0); 
        
        assert_eq!(price_data.num_, 1);
        assert_eq!(price_data.comp_[0].pub_, publisher.pubkey());

        assert_eq!(price_data.next_price_account, Pubkey::default());
    }

    let mut quotes : HashMap<String, Quote> = HashMap::new();
    for (key, price) in &price_accounts {
        quotes.insert(key.to_string(), Quote { price: rand::random::<i64>() % 150 + 1, confidence: rand::random::<u64>() % 20 + 1, status: PC_STATUS_TRADING, slot_diff : 0});
    }

    sim.upd_price_batch(&publisher, &price_accounts,&quotes).await.unwrap();
    
    for (key, price) in &price_accounts {
        let price_data = sim.get_account_data_as::<PriceAccount>(*price).await.unwrap();
        let quote = quotes.get(key).unwrap();
        assert_eq!(price_data.comp_[0].latest_.price_, quote.price);
        assert_eq!(price_data.comp_[0].latest_.conf_, quote.confidence);
        println!("{} {}", quote.price, quote.confidence);
        println!("status : {}", check_confidence_too_big(quote.price, quote.confidence, quote.status).unwrap());
        assert_eq!(price_data.comp_[0].latest_.status_, check_confidence_too_big(quote.price, quote.confidence, quote.status).unwrap());
    }


}
