use crate::deserialize::load_mut;
use crate::rust_oracle::{
    del_publisher,
    pubkey_assign,
    pubkey_is_zero,
};
use crate::tests::test_utils::AccountSetup;
use bytemuck::bytes_of;
use solana_program::pubkey::Pubkey;

use crate::c_oracle_header::{
    cmd_del_publisher,
    command_t_e_cmd_del_publisher,
    pc_price_comp_t,
    pc_price_info_t,
    pc_price_t,
    pc_pub_key_t,
    PythAccount,
    PC_STATUS_TRADING,
    PC_VERSION,
};
use std::mem::size_of;

use crate::rust_oracle::{
    initialize_checked,
    load_checked,
    pubkey_equal,
};

#[test]
fn test_del_publisher() {
    let p1: pc_price_info_t = pc_price_info_t {
        price_:           100,
        conf_:            10,
        status_:          PC_STATUS_TRADING,
        pub_slot_:        42,
        corp_act_status_: 0,
    };

    let p2: pc_price_info_t = pc_price_info_t {
        price_:           200,
        conf_:            20,
        status_:          PC_STATUS_TRADING,
        pub_slot_:        42,
        corp_act_status_: 0,
    };

    let program_id = Pubkey::new_unique();
    let publisher = pc_pub_key_t::new_unique();
    let publisher2 = pc_pub_key_t::new_unique();

    let mut instruction_data = [0u8; size_of::<cmd_del_publisher>()];
    let mut hdr = load_mut::<cmd_del_publisher>(&mut instruction_data).unwrap();
    hdr.ver_ = PC_VERSION;
    hdr.cmd_ = command_t_e_cmd_del_publisher as i32;
    hdr.pub_ = publisher;

    let mut funding_setup = AccountSetup::new_funding();
    let funding_account = funding_setup.to_account_info();

    let mut price_setup = AccountSetup::new::<pc_price_t>(&program_id);
    let price_account = price_setup.to_account_info();
    initialize_checked::<pc_price_t>(&price_account, PC_VERSION).unwrap();
    {
        let mut price_data = load_checked::<pc_price_t>(&price_account, PC_VERSION).unwrap();
        price_data.num_ = 1;
        price_data.comp_[0].latest_ = p1;
        pubkey_assign(&mut price_data.comp_[0].pub_, bytes_of(&publisher));
    }

    assert!(del_publisher(
        &program_id,
        &[funding_account.clone(), price_account.clone(),],
        &instruction_data
    )
    .is_ok());

    {
        let mut price_data = load_checked::<pc_price_t>(&price_account, PC_VERSION).unwrap();
        assert_eq!(price_data.num_, 0);
        assert_eq!(price_data.comp_[0].latest_.price_, 0);
        assert_eq!(price_data.comp_[0].latest_.conf_, 0);
        assert_eq!(price_data.comp_[0].latest_.status_, 0);
        assert_eq!(price_data.comp_[0].latest_.pub_slot_, 0);
        assert_eq!(price_data.comp_[0].latest_.corp_act_status_, 0);
        assert_eq!(price_data.size_, pc_price_t::INITIAL_SIZE);
        assert!(pubkey_is_zero(&price_data.comp_[0].pub_));

        price_data.num_ = 2;
        price_data.comp_[0].latest_ = p1;
        price_data.comp_[1].latest_ = p2;
        pubkey_assign(&mut price_data.comp_[0].pub_, bytes_of(&publisher));
        pubkey_assign(&mut price_data.comp_[1].pub_, bytes_of(&publisher2));
    }

    // Delete publisher at position 0
    assert!(del_publisher(
        &program_id,
        &[funding_account.clone(), price_account.clone(),],
        &instruction_data
    )
    .is_ok());

    {
        let mut price_data = load_checked::<pc_price_t>(&price_account, PC_VERSION).unwrap();
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
            pc_price_t::INITIAL_SIZE + (size_of::<pc_price_comp_t>() as u32)
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
    assert!(del_publisher(
        &program_id,
        &[funding_account.clone(), price_account.clone(),],
        &instruction_data
    )
    .is_ok());

    {
        let mut price_data = load_checked::<pc_price_t>(&price_account, PC_VERSION).unwrap();
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
            pc_price_t::INITIAL_SIZE + (size_of::<pc_price_comp_t>() as u32)
        );
        assert!(pubkey_equal(
            &price_data.comp_[0].pub_,
            bytes_of(&publisher2)
        ));
        assert!(pubkey_is_zero(&price_data.comp_[1].pub_));
    }
}
