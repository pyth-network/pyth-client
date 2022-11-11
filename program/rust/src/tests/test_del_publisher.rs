use {
    crate::{
        accounts::{
            PriceAccount,
            PriceComponent,
            PriceInfo,
            PythAccount,
        },
        c_oracle_header::{
            PC_STATUS_TRADING,
            PC_VERSION,
        },
        deserialize::{
            load_checked,
            load_mut,
        },
        instruction::{
            DelPublisherArgs,
            OracleCommand,
        },
        processor::process_instruction,
        tests::test_utils::AccountSetup,
    },
    solana_program::pubkey::Pubkey,
    std::mem::size_of,
};

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
    let publisher = Pubkey::new_unique();
    let publisher2 = Pubkey::new_unique();

    let mut instruction_data = [0u8; size_of::<DelPublisherArgs>()];
    let mut hdr = load_mut::<DelPublisherArgs>(&mut instruction_data).unwrap();
    hdr.header = OracleCommand::DelPublisher.into();
    hdr.publisher = publisher;

    let mut funding_setup = AccountSetup::new_funding();
    let funding_account = funding_setup.as_account_info();

    let mut price_setup = AccountSetup::new::<PriceAccount>(&program_id);
    let price_account = price_setup.as_account_info();
    PriceAccount::initialize(&price_account, PC_VERSION).unwrap();
    {
        let mut price_data = load_checked::<PriceAccount>(&price_account, PC_VERSION).unwrap();
        price_data.num_ = 1;
        price_data.comp_[0].latest_ = p1;
        price_data.comp_[0].pub_ = publisher
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
        assert_eq!(price_data.header.size, PriceAccount::INITIAL_SIZE);
        assert!(price_data.comp_[0].pub_ == Pubkey::default());

        price_data.num_ = 2;
        price_data.comp_[0].latest_ = p1;
        price_data.comp_[1].latest_ = p2;
        price_data.comp_[0].pub_ = publisher;
        price_data.comp_[1].pub_ = publisher2;
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
            price_data.header.size,
            PriceAccount::INITIAL_SIZE + (size_of::<PriceComponent>() as u32)
        );
        assert!(price_data.comp_[0].pub_ == publisher2);
        assert!(price_data.comp_[1].pub_ == Pubkey::default());

        price_data.num_ = 2;
        price_data.comp_[0].latest_ = p2;
        price_data.comp_[1].latest_ = p1;
        price_data.comp_[0].pub_ = publisher2;
        price_data.comp_[1].pub_ = publisher;
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
            price_data.header.size,
            PriceAccount::INITIAL_SIZE + (size_of::<PriceComponent>() as u32)
        );
        assert!(price_data.comp_[0].pub_ == publisher2);
        assert!(price_data.comp_[1].pub_ == Pubkey::default());
    }
}
