use crate::c_oracle_header::{
    CPubkey,
    PriceAccount,
    PriceComponent,
    PriceInfo,
    PythAccount,
    PC_STATUS_TRADING,
    PC_VERSION,
};
use crate::deserialize::{
    initialize_pyth_account_checked,
    load_checked,
    load_mut,
};
use crate::instruction::{
    DelPublisherArgs,
    OracleCommand,
};
use crate::processor::process_instruction;
use crate::tests::test_utils::AccountSetup;
use crate::utils::{
    pubkey_assign,
    pubkey_equal,
    pubkey_is_zero,
};
use bytemuck::bytes_of;
use solana_program::pubkey::Pubkey;
use std::mem::size_of;

#[test]
fn test_del_publisher() {
    let p1: PriceInfo = PriceInfo {
        price_:           100,
        conf_:            10,
        status_:          PC_STATUS_TRADING,
        pub_slot_:        42,
        corp_act_status_: 0,
    };

    let p2: PriceInfo = PriceInfo {
        price_:           200,
        conf_:            20,
        status_:          PC_STATUS_TRADING,
        pub_slot_:        42,
        corp_act_status_: 0,
    };

    let program_id = Pubkey::new_unique();
    let publisher = CPubkey::new_unique();
    let publisher2 = CPubkey::new_unique();

    let mut instruction_data = [0u8; size_of::<DelPublisherArgs>()];
    let mut hdr = load_mut::<DelPublisherArgs>(&mut instruction_data).unwrap();
    hdr.header = OracleCommand::DelPublisher.into();
    hdr.publisher = publisher;

    let mut funding_setup = AccountSetup::new_funding();
    let funding_account = funding_setup.to_account_info();

    let mut price_setup = AccountSetup::new::<PriceAccount>(&program_id);
    let price_account = price_setup.to_account_info();
    initialize_pyth_account_checked::<PriceAccount>(&price_account, PC_VERSION).unwrap();
    {
        let mut price_data = load_checked::<PriceAccount>(&price_account, PC_VERSION).unwrap();
        price_data.num_ = 1;
        price_data.comp_[0].latest_ = p1;
        pubkey_assign(&mut price_data.comp_[0].pub_, bytes_of(&publisher));
    }

    assert!(process_instruction(
        &program_id,
        &[funding_account.clone(), price_account.clone(),],
        &instruction_data
    )
    .is_ok());

    {
        let mut price_data = load_checked::<PriceAccount>(&price_account, PC_VERSION).unwrap();
        assert_eq!(price_data.num_, 0);
        assert_eq!(price_data.comp_[0].latest_.price_, 0);
        assert_eq!(price_data.comp_[0].latest_.conf_, 0);
        assert_eq!(price_data.comp_[0].latest_.status_, 0);
        assert_eq!(price_data.comp_[0].latest_.pub_slot_, 0);
        assert_eq!(price_data.comp_[0].latest_.corp_act_status_, 0);
        assert_eq!(price_data.size_, PriceAccount::INITIAL_SIZE);
        assert!(pubkey_is_zero(&price_data.comp_[0].pub_));

        price_data.num_ = 2;
        price_data.comp_[0].latest_ = p1;
        price_data.comp_[1].latest_ = p2;
        pubkey_assign(&mut price_data.comp_[0].pub_, bytes_of(&publisher));
        pubkey_assign(&mut price_data.comp_[1].pub_, bytes_of(&publisher2));
    }

    // Delete publisher at position 0
    assert!(process_instruction(
        &program_id,
        &[funding_account.clone(), price_account.clone(),],
        &instruction_data
    )
    .is_ok());

    {
        let mut price_data = load_checked::<PriceAccount>(&price_account, PC_VERSION).unwrap();
        assert_eq!(price_data.num_, 1);
        assert_eq!(price_data.comp_[0].latest_.price_, p2.price_);
        assert_eq!(price_data.comp_[0].latest_.conf_, p2.conf_);
        assert_eq!(price_data.comp_[0].latest_.status_, p2.status_);
        assert_eq!(price_data.comp_[0].latest_.pub_slot_, p2.pub_slot_);
        assert_eq!(
            price_data.comp_[0].latest_.corp_act_status_,
            p2.corp_act_status_
        );
        assert_eq!(price_data.comp_[1].latest_.price_, 0);
        assert_eq!(price_data.comp_[1].latest_.conf_, 0);
        assert_eq!(price_data.comp_[1].latest_.status_, 0);
        assert_eq!(price_data.comp_[1].latest_.pub_slot_, 0);
        assert_eq!(price_data.comp_[1].latest_.corp_act_status_, 0);
        assert_eq!(
            price_data.size_,
            PriceAccount::INITIAL_SIZE + (size_of::<PriceComponent>() as u32)
        );
        assert!(pubkey_equal(
            &price_data.comp_[0].pub_,
            bytes_of(&publisher2)
        ));
        assert!(pubkey_is_zero(&price_data.comp_[1].pub_));

        price_data.num_ = 2;
        price_data.comp_[0].latest_ = p2;
        price_data.comp_[1].latest_ = p1;
        pubkey_assign(&mut price_data.comp_[0].pub_, bytes_of(&publisher2));
        pubkey_assign(&mut price_data.comp_[1].pub_, bytes_of(&publisher));
    }

    // Delete publisher at position 1
    assert!(process_instruction(
        &program_id,
        &[funding_account.clone(), price_account.clone(),],
        &instruction_data
    )
    .is_ok());

    {
        let price_data = load_checked::<PriceAccount>(&price_account, PC_VERSION).unwrap();
        assert_eq!(price_data.num_, 1);
        assert_eq!(price_data.comp_[0].latest_.price_, p2.price_);
        assert_eq!(price_data.comp_[0].latest_.conf_, p2.conf_);
        assert_eq!(price_data.comp_[0].latest_.status_, p2.status_);
        assert_eq!(price_data.comp_[0].latest_.pub_slot_, p2.pub_slot_);
        assert_eq!(
            price_data.comp_[0].latest_.corp_act_status_,
            p2.corp_act_status_
        );
        assert_eq!(price_data.comp_[1].latest_.price_, 0);
        assert_eq!(price_data.comp_[1].latest_.conf_, 0);
        assert_eq!(price_data.comp_[1].latest_.status_, 0);
        assert_eq!(price_data.comp_[1].latest_.pub_slot_, 0);
        assert_eq!(price_data.comp_[1].latest_.corp_act_status_, 0);
        assert_eq!(
            price_data.size_,
            PriceAccount::INITIAL_SIZE + (size_of::<PriceComponent>() as u32)
        );
        assert!(pubkey_equal(
            &price_data.comp_[0].pub_,
            bytes_of(&publisher2)
        ));
        assert!(pubkey_is_zero(&price_data.comp_[1].pub_));
    }
}
