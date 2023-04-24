use solana_program::native_token::LAMPORTS_PER_SOL;
use solana_sdk::signature::Keypair;
use crate::tests::pyth_simulator::Quote;
use crate::{deserialize::load_checked, accounts::PriceAccount};
use crate::c_oracle_header::PC_VERSION;
use crate::c_oracle_header::PC_STATUS_UNKNOWN;
use crate::c_oracle_header::PC_STATUS_TRADING;
use crate::c_oracle_header::PRICE_ACCOUNT_DEFAULT_MIN_PUB;
use {
    crate::{
        accounts::MappingAccount,
        tests::pyth_simulator::PythSimulator,
    },
    solana_program::pubkey::Pubkey,
    solana_sdk::signer::Signer,
    std::mem::{
        size_of,
        size_of_val,
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
    let price_data = sim.get_account_data_as::<PriceAccount>(price).await.unwrap();
    
    assert_eq!(price_data.exponent, -5);
    assert_eq!(price_data.price_type, 1);
    assert_eq!(price_data.min_pub_, PRICE_ACCOUNT_DEFAULT_MIN_PUB);

    assert_eq!(price_data.agg_.status_, PC_STATUS_UNKNOWN);
    assert_eq!(price_data.agg_.price_, 0);
    assert_eq!(price_data.agg_.conf_, 0); 
    
    assert_eq!(price_data.num_, 1);
    assert_eq!(price_data.comp_[0].pub_, publisher.pubkey());
    }

    sim.upd_price(&publisher, price , Some(Quote { price: 150, confidence: 7, status: PC_STATUS_TRADING, slot_diff : 0})).await.unwrap();
    sim.upd_price(&publisher, price , None).await.unwrap(); //Trigger update


    {
        let price_data = sim.get_account_data_as::<PriceAccount>(price).await.unwrap();
        
        assert_eq!(price_data.comp_[0].latest_.price_, 150);
        assert_eq!(price_data.comp_[0].latest_.conf_, 7);
        assert_eq!(price_data.comp_[0].latest_.status_, PC_STATUS_TRADING);
        
        }

    sim.upd_price(&publisher, price , Some(Quote { price: 100, confidence: 1, status: PC_STATUS_TRADING, slot_diff : 1})).await.unwrap();
    sim.upd_price(&publisher, price , None).await.unwrap(); //Trigger update

    {
        let price_data = sim.get_account_data_as::<PriceAccount>(price).await.unwrap();
        
        assert_eq!(price_data.comp_[0].latest_.price_, 100);
        assert_eq!(price_data.comp_[0].latest_.conf_, 1);
        assert_eq!(price_data.comp_[0].latest_.status_, PC_STATUS_TRADING);
        
        }
    
}
