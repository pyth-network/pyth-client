use {
    crate::{
        accounts::{
            PriceAccount,
            PriceInfo,
            PythAccount,
        },
        c_oracle_header::{
            PC_STATUS_TRADING,
            PC_STATUS_UNKNOWN,
            PC_VERSION,
        },
        deserialize::{
            load_checked,
            load_mut,
        },
        instruction::{
            OracleCommand,
            UpdPriceArgs,
        },
        processor::c_upd_aggregate,
        tests::test_utils::AccountSetup,
    },
    solana_program::pubkey::Pubkey,
    std::mem::size_of,
};

#[test]
fn test_upd_aggregate() {
    let p1: PriceInfo = PriceInfo {
        price_:           100,
        conf_:            10,
        status_:          PC_STATUS_TRADING,
        pub_slot_:        1000,
        corp_act_status_: 0,
    };

    let p2: PriceInfo = PriceInfo {
        price_:           200,
        conf_:            20,
        status_:          PC_STATUS_TRADING,
        pub_slot_:        1000,
        corp_act_status_: 0,
    };

    let p3: PriceInfo = PriceInfo {
        price_:           300,
        conf_:            30,
        status_:          PC_STATUS_TRADING,
        pub_slot_:        1000,
        corp_act_status_: 0,
    };

    let p4: PriceInfo = PriceInfo {
        price_:           400,
        conf_:            40,
        status_:          PC_STATUS_TRADING,
        pub_slot_:        1000,
        corp_act_status_: 0,
    };

    let p5: PriceInfo = PriceInfo {
        price_:           500,
        conf_:            50,
        status_:          PC_STATUS_TRADING,
        pub_slot_:        1050,
        corp_act_status_: 0,
    };

    let p6: PriceInfo = PriceInfo {
        price_:           600,
        conf_:            60,
        status_:          PC_STATUS_TRADING,
        pub_slot_:        1074,
        corp_act_status_: 0,
    };

    let mut instruction_data = [0u8; size_of::<UpdPriceArgs>()];
    populate_instruction(&mut instruction_data, 42, 2, 1);

    let program_id = Pubkey::new_unique();

    let mut price_setup = AccountSetup::new::<PriceAccount>(&program_id);
    let mut price_account = price_setup.as_account_info();
    price_account.is_signer = false;
    PriceAccount::initialize(&price_account, PC_VERSION).unwrap();

    // single publisher
    {
        let mut price_data = load_checked::<PriceAccount>(&price_account, PC_VERSION).unwrap();
        price_data.num_ = 1;
        price_data.last_slot_ = 1000;
        price_data.agg_.pub_slot_ = 1000;
        price_data.comp_[0].latest_ = p1;
    }

    unsafe {
        assert!(c_upd_aggregate(
            price_account.try_borrow_mut_data().unwrap().as_mut_ptr(),
            1001,
            1,
        ));
    }

    {
        let price_data = load_checked::<PriceAccount>(&price_account, PC_VERSION).unwrap();

        assert_eq!(price_data.agg_.price_, 100);
        assert_eq!(price_data.agg_.conf_, 10);
        assert_eq!(price_data.twap_.val_, 100);
        assert_eq!(price_data.twac_.val_, 10);
        assert_eq!(price_data.num_qt_, 1);
        assert_eq!(price_data.timestamp_, 1);
        assert_eq!(price_data.prev_slot_, 0);
        assert_eq!(price_data.prev_price_, 0);
        assert_eq!(price_data.prev_conf_, 0);
        assert_eq!(price_data.prev_timestamp_, 0);
    }

    // two publishers
    {
        let mut price_data = load_checked::<PriceAccount>(&price_account, PC_VERSION).unwrap();

        price_data.num_ = 2;
        price_data.last_slot_ = 1000;
        price_data.agg_.pub_slot_ = 1000;
        price_data.comp_[0].latest_ = p1;
        price_data.comp_[1].latest_ = p2;
    }

    unsafe {
        assert!(c_upd_aggregate(
            price_account.try_borrow_mut_data().unwrap().as_mut_ptr(),
            1001,
            2,
        ));
    }

    {
        let price_data = load_checked::<PriceAccount>(&price_account, PC_VERSION).unwrap();

        assert_eq!(price_data.agg_.price_, 145);
        assert_eq!(price_data.agg_.conf_, 55);
        assert_eq!(price_data.twap_.val_, 106);
        assert_eq!(price_data.twac_.val_, 16);
        assert_eq!(price_data.num_qt_, 2);
        assert_eq!(price_data.timestamp_, 2);
        assert_eq!(price_data.prev_slot_, 1000);
        assert_eq!(price_data.prev_price_, 100);
        assert_eq!(price_data.prev_conf_, 10);
        assert_eq!(price_data.prev_timestamp_, 1);
    }

    // three publishers
    {
        let mut price_data = load_checked::<PriceAccount>(&price_account, PC_VERSION).unwrap();

        price_data.num_ = 3;
        price_data.last_slot_ = 1000;
        price_data.agg_.pub_slot_ = 1000;
        price_data.comp_[0].latest_ = p1;
        price_data.comp_[1].latest_ = p2;
        price_data.comp_[2].latest_ = p3;
    }

    unsafe {
        assert!(c_upd_aggregate(
            price_account.try_borrow_mut_data().unwrap().as_mut_ptr(),
            1001,
            3,
        ));
    }

    {
        let price_data = load_checked::<PriceAccount>(&price_account, PC_VERSION).unwrap();

        assert_eq!(price_data.agg_.price_, 200);
        assert_eq!(price_data.agg_.conf_, 90);
        assert_eq!(price_data.twap_.val_, 114);
        assert_eq!(price_data.twac_.val_, 23);
        assert_eq!(price_data.num_qt_, 3);
        assert_eq!(price_data.timestamp_, 3);
        assert_eq!(price_data.prev_slot_, 1000);
        assert_eq!(price_data.prev_price_, 145);
        assert_eq!(price_data.prev_conf_, 55);
        assert_eq!(price_data.prev_timestamp_, 2);
    }

    // four publishers
    {
        let mut price_data = load_checked::<PriceAccount>(&price_account, PC_VERSION).unwrap();

        price_data.num_ = 4;
        price_data.last_slot_ = 1000;
        price_data.agg_.pub_slot_ = 1000;
        price_data.comp_[0].latest_ = p1;
        price_data.comp_[1].latest_ = p2;
        price_data.comp_[2].latest_ = p3;
        price_data.comp_[3].latest_ = p4;
    }

    unsafe {
        assert!(c_upd_aggregate(
            price_account.try_borrow_mut_data().unwrap().as_mut_ptr(),
            1001,
            4,
        ));
    }

    {
        let price_data = load_checked::<PriceAccount>(&price_account, PC_VERSION).unwrap();

        assert_eq!(price_data.agg_.price_, 245);
        assert_eq!(price_data.agg_.conf_, 85);
        assert_eq!(price_data.twap_.val_, 125);
        assert_eq!(price_data.twac_.val_, 28);
        assert_eq!(price_data.num_qt_, 4);
        assert_eq!(price_data.timestamp_, 4);
        assert_eq!(price_data.last_slot_, 1001);
        assert_eq!(price_data.prev_slot_, 1000);
        assert_eq!(price_data.prev_price_, 200);
        assert_eq!(price_data.prev_conf_, 90);
        assert_eq!(price_data.prev_timestamp_, 3);
    }

    unsafe {
        assert!(c_upd_aggregate(
            price_account.try_borrow_mut_data().unwrap().as_mut_ptr(),
            1025,
            5,
        ));
    }

    {
        let price_data = load_checked::<PriceAccount>(&price_account, PC_VERSION).unwrap();

        assert_eq!(price_data.agg_.status_, PC_STATUS_TRADING);
        assert_eq!(price_data.last_slot_, 1025);
        assert_eq!(price_data.num_qt_, 4);
        assert_eq!(price_data.timestamp_, 5);
        assert_eq!(price_data.prev_slot_, 1001);
        assert_eq!(price_data.prev_price_, 245);
        assert_eq!(price_data.prev_conf_, 85);
        assert_eq!(price_data.prev_timestamp_, 4);
    }

    // check what happens when nothing publishes for a while
    unsafe {
        assert!(!c_upd_aggregate(
            price_account.try_borrow_mut_data().unwrap().as_mut_ptr(),
            1026,
            10,
        ));
    }

    {
        let price_data = load_checked::<PriceAccount>(&price_account, PC_VERSION).unwrap();

        assert_eq!(price_data.agg_.status_, PC_STATUS_UNKNOWN);
        assert_eq!(price_data.last_slot_, 1025);
        assert_eq!(price_data.num_qt_, 0);
        assert_eq!(price_data.timestamp_, 10);
        assert_eq!(price_data.prev_slot_, 1025);
        assert_eq!(price_data.prev_price_, 245);
        assert_eq!(price_data.prev_conf_, 85);
        assert_eq!(price_data.prev_timestamp_, 5);
    }

    unsafe {
        assert!(!c_upd_aggregate(
            price_account.try_borrow_mut_data().unwrap().as_mut_ptr(),
            1028,
            12,
        ));
    }

    {
        let price_data = load_checked::<PriceAccount>(&price_account, PC_VERSION).unwrap();

        assert_eq!(price_data.agg_.status_, PC_STATUS_UNKNOWN);
        assert_eq!(price_data.last_slot_, 1025);
        assert_eq!(price_data.num_qt_, 0);
        assert_eq!(price_data.timestamp_, 12);
        assert_eq!(price_data.prev_slot_, 1025);
        assert_eq!(price_data.prev_price_, 245);
        assert_eq!(price_data.prev_conf_, 85);
        assert_eq!(price_data.prev_timestamp_, 5);
    }

    // check max_latency_ = 0 defaults to PC_MAX_SEND_LATENCY
    {
        let mut price_data = load_checked::<PriceAccount>(&price_account, PC_VERSION).unwrap();
        price_data.num_ = 1;
        price_data.agg_.pub_slot_ = 1050;
        price_data.comp_[0].latest_ = p5;
    }

    unsafe {
        assert!(c_upd_aggregate(
            price_account.try_borrow_mut_data().unwrap().as_mut_ptr(),
            1051,
            13,
        ));
    }

    {
        let price_data = load_checked::<PriceAccount>(&price_account, PC_VERSION).unwrap();

        assert_eq!(price_data.max_latency_, 0);
        assert_eq!(price_data.agg_.price_, 500);
        assert_eq!(price_data.agg_.conf_, 50);
        assert_eq!(price_data.twap_.val_, 177);
        assert_eq!(price_data.twac_.val_, 34);
        assert_eq!(price_data.num_qt_, 1);
        assert_eq!(price_data.timestamp_, 13);
        assert_eq!(price_data.prev_slot_, 1025);
        assert_eq!(price_data.prev_price_, 245);
        assert_eq!(price_data.prev_conf_, 85);
        assert_eq!(price_data.prev_timestamp_, 5);
    }

    // ensure the update occurs within the PC_MAX_SEND_LATENCY limit of 25 slots, allowing the aggregated price to reflect both p5 and p6 contributions
    {
        let mut price_data = load_checked::<PriceAccount>(&price_account, PC_VERSION).unwrap();
        price_data.num_ = 2;
        price_data.last_slot_ = 1025;
        price_data.agg_.pub_slot_ = 1050;
        price_data.comp_[0].latest_ = p5;
        price_data.comp_[1].latest_ = p6;
    }

    unsafe {
        assert!(c_upd_aggregate(
            price_account.try_borrow_mut_data().unwrap().as_mut_ptr(),
            1075,
            14,
        ));
    }

    {
        let price_data = load_checked::<PriceAccount>(&price_account, PC_VERSION).unwrap();

        assert_eq!(price_data.max_latency_, 0);
        assert_eq!(price_data.agg_.price_, 545);
        assert_eq!(price_data.agg_.conf_, 55);
        assert_eq!(price_data.twap_.val_, 212);
        assert_eq!(price_data.twac_.val_, 36);
        assert_eq!(price_data.num_qt_, 2);
        assert_eq!(price_data.timestamp_, 14);
        assert_eq!(price_data.prev_slot_, 1050);
        assert_eq!(price_data.prev_price_, 500);
        assert_eq!(price_data.prev_conf_, 50);
        assert_eq!(price_data.prev_timestamp_, 13);
    }

    // verify behavior when publishing halts for 1 slot, causing the slot difference from p5 to exceed the PC_MAX_SEND_LATENCY threshold of 25.
    unsafe {
        assert!(c_upd_aggregate(
            price_account.try_borrow_mut_data().unwrap().as_mut_ptr(),
            1076,
            15,
        ));
    }

    {
        let price_data = load_checked::<PriceAccount>(&price_account, PC_VERSION).unwrap();

        assert_eq!(price_data.max_latency_, 0);
        assert_eq!(price_data.agg_.price_, 600);
        assert_eq!(price_data.agg_.conf_, 60);
        assert_eq!(price_data.twap_.val_, 244);
        assert_eq!(price_data.twac_.val_, 38);
        assert_eq!(price_data.num_qt_, 1);
        assert_eq!(price_data.timestamp_, 15);
        assert_eq!(price_data.prev_slot_, 1075);
        assert_eq!(price_data.prev_price_, 545);
        assert_eq!(price_data.prev_conf_, 55);
        assert_eq!(price_data.prev_timestamp_, 14);
    }

    // verify behavior when max_latency_ is set to 5, this should result in PC_STATUS_UNKNOWN status
    {
        let mut price_data = load_checked::<PriceAccount>(&price_account, PC_VERSION).unwrap();
        price_data.max_latency_ = 5;
        price_data.num_ = 2;
        price_data.last_slot_ = 1025;
        price_data.agg_.pub_slot_ = 1050;
        price_data.comp_[0].latest_ = p5;
        price_data.comp_[1].latest_ = p6;
    }

    unsafe {
        assert!(!c_upd_aggregate(
            price_account.try_borrow_mut_data().unwrap().as_mut_ptr(),
            1080,
            16,
        ));
    }

    {
        let price_data = load_checked::<PriceAccount>(&price_account, PC_VERSION).unwrap();

        assert_eq!(price_data.max_latency_, 5);
        assert_eq!(price_data.agg_.status_, PC_STATUS_UNKNOWN);
        assert_eq!(price_data.agg_.price_, 600);
        assert_eq!(price_data.agg_.conf_, 60);
        assert_eq!(price_data.twap_.val_, 244);
        assert_eq!(price_data.twac_.val_, 38);
        assert_eq!(price_data.num_qt_, 0);
        assert_eq!(price_data.timestamp_, 16);
        assert_eq!(price_data.prev_slot_, 1050);
        assert_eq!(price_data.prev_price_, 600);
        assert_eq!(price_data.prev_conf_, 60);
        assert_eq!(price_data.prev_timestamp_, 15);
    }
}

// Create an upd_price instruction with the provided parameters
fn populate_instruction(instruction_data: &mut [u8], price: i64, conf: u64, pub_slot: u64) {
    let mut cmd = load_mut::<UpdPriceArgs>(instruction_data).unwrap();
    cmd.header = OracleCommand::AggPrice.into();
    cmd.status = PC_STATUS_TRADING;
    cmd.price = price;
    cmd.confidence = conf;
    cmd.publishing_slot = pub_slot;
    cmd.unused_ = 0;
}
