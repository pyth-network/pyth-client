// use crate::processor::process_instruction;
use {
    crate::{
        c_oracle_header::{
            PC_MAX_SEND_LATENCY,
            PC_STATUS_TRADING,
            PC_STATUS_UNKNOWN,
            PC_VERSION,
        },
        deserialize::{
            initialize_pyth_account_checked,
            load_checked,
            load_mut,
        },
        instruction::{
            OracleCommand,
            UpdPriceArgs,
        },
        processor::process_instruction,
        tests::test_utils::{
            update_clock_slot,
            AccountSetup,
        },
        time_machine_types::{
            PriceAccountWrapper,
            THIRTY_MINUTES,
        },
    },
    solana_program::{
        program_error::ProgramError,
        pubkey::Pubkey,
    },
    std::mem::size_of,
};

/// Clone of test_upd_price that also checks sma fields
#[test]
fn test_upd_sma() {
    let mut instruction_data = [0u8; size_of::<UpdPriceArgs>()];
    populate_instruction(&mut instruction_data, 42, 2, 1);

    let program_id = Pubkey::new_unique();

    let mut funding_setup = AccountSetup::new_funding();
    let funding_account = funding_setup.as_account_info();

    let mut price_setup = AccountSetup::new::<PriceAccountWrapper>(&program_id);
    let mut price_account = price_setup.as_account_info();
    price_account.is_signer = false;
    initialize_pyth_account_checked::<PriceAccountWrapper>(&price_account, PC_VERSION).unwrap();


    {
        let mut price_data =
            load_checked::<PriceAccountWrapper>(&price_account, PC_VERSION).unwrap();
        price_data
            .time_machine
            .initialize(THIRTY_MINUTES, PC_MAX_SEND_LATENCY as u64);

        price_data.price_data.num_ = 1;
        price_data.price_data.comp_[0].pub_ = *funding_account.key;
    }

    let mut clock_setup = AccountSetup::new_clock();
    let mut clock_account = clock_setup.as_account_info();
    clock_account.is_signer = false;
    clock_account.is_writable = false;

    update_clock_slot(&mut clock_account, 1);

    assert!(process_instruction(
        &program_id,
        &[
            funding_account.clone(),
            price_account.clone(),
            clock_account.clone()
        ],
        &instruction_data
    )
    .is_ok());

    {
        let price_data = load_checked::<PriceAccountWrapper>(&price_account, PC_VERSION).unwrap();
        assert_eq!(price_data.price_data.comp_[0].latest_.price_, 42);
        assert_eq!(price_data.price_data.comp_[0].latest_.conf_, 2);
        assert_eq!(price_data.price_data.comp_[0].latest_.pub_slot_, 1);
        assert_eq!(
            price_data.price_data.comp_[0].latest_.status_,
            PC_STATUS_TRADING
        );
        assert_eq!(price_data.price_data.valid_slot_, 0);
        assert_eq!(price_data.price_data.agg_.pub_slot_, 1);
        assert_eq!(price_data.price_data.agg_.price_, 0);
        assert_eq!(price_data.price_data.agg_.status_, PC_STATUS_UNKNOWN);

        assert_eq!(price_data.time_machine.current_epoch_numerator, 0);
        assert!(!price_data.time_machine.current_epoch_is_valid);
        assert_eq!(price_data.time_machine.current_epoch_denominator, 0);
    }

    // add some prices for current slot - get rejected
    populate_instruction(&mut instruction_data, 43, 2, 1);

    assert_eq!(
        process_instruction(
            &program_id,
            &[
                funding_account.clone(),
                price_account.clone(),
                clock_account.clone()
            ],
            &instruction_data
        ),
        Err(ProgramError::InvalidArgument)
    );

    {
        let price_data = load_checked::<PriceAccountWrapper>(&price_account, PC_VERSION).unwrap();
        assert_eq!(price_data.price_data.comp_[0].latest_.price_, 42);
        assert_eq!(price_data.price_data.comp_[0].latest_.conf_, 2);
        assert_eq!(price_data.price_data.comp_[0].latest_.pub_slot_, 1);
        assert_eq!(
            price_data.price_data.comp_[0].latest_.status_,
            PC_STATUS_TRADING
        );
        assert_eq!(price_data.price_data.valid_slot_, 0);
        assert_eq!(price_data.price_data.agg_.pub_slot_, 1);
        assert_eq!(price_data.price_data.agg_.price_, 0);
        assert_eq!(price_data.price_data.agg_.status_, PC_STATUS_UNKNOWN);

        assert_eq!(price_data.time_machine.current_epoch_numerator, 0);
        assert!(!price_data.time_machine.current_epoch_is_valid);
        assert_eq!(price_data.time_machine.current_epoch_denominator, 0);
    }

    // add next price in new slot triggering snapshot and aggregate calc
    populate_instruction(&mut instruction_data, 81, 2, 2);
    update_clock_slot(&mut clock_account, 3);

    assert!(process_instruction(
        &program_id,
        &[
            funding_account.clone(),
            price_account.clone(),
            clock_account.clone()
        ],
        &instruction_data
    )
    .is_ok());

    {
        let price_data = load_checked::<PriceAccountWrapper>(&price_account, PC_VERSION).unwrap();
        assert_eq!(price_data.price_data.comp_[0].latest_.price_, 81);
        assert_eq!(price_data.price_data.comp_[0].latest_.conf_, 2);
        assert_eq!(price_data.price_data.comp_[0].latest_.pub_slot_, 2);
        assert_eq!(
            price_data.price_data.comp_[0].latest_.status_,
            PC_STATUS_TRADING
        );
        assert_eq!(price_data.price_data.valid_slot_, 1);
        assert_eq!(price_data.price_data.agg_.pub_slot_, 3);
        assert_eq!(price_data.price_data.agg_.price_, 42);
        assert_eq!(price_data.price_data.agg_.status_, PC_STATUS_TRADING);

        assert_eq!(price_data.time_machine.current_epoch_numerator, 42 / 2 * 3);
        assert!(!price_data.time_machine.current_epoch_is_valid);
        assert_eq!(price_data.time_machine.current_epoch_denominator, 3);
    }

    // next price doesnt change but slot does
    populate_instruction(&mut instruction_data, 81, 2, 3);
    update_clock_slot(&mut clock_account, 4);
    assert!(process_instruction(
        &program_id,
        &[
            funding_account.clone(),
            price_account.clone(),
            clock_account.clone()
        ],
        &instruction_data
    )
    .is_ok());

    {
        let price_data = load_checked::<PriceAccountWrapper>(&price_account, PC_VERSION).unwrap();
        assert_eq!(price_data.price_data.comp_[0].latest_.price_, 81);
        assert_eq!(price_data.price_data.comp_[0].latest_.conf_, 2);
        assert_eq!(price_data.price_data.comp_[0].latest_.pub_slot_, 3);
        assert_eq!(
            price_data.price_data.comp_[0].latest_.status_,
            PC_STATUS_TRADING
        );
        assert_eq!(price_data.price_data.valid_slot_, 3);
        assert_eq!(price_data.price_data.agg_.pub_slot_, 4);
        assert_eq!(price_data.price_data.agg_.price_, 81);
        assert_eq!(price_data.price_data.agg_.status_, PC_STATUS_TRADING);

        assert_eq!(
            price_data.time_machine.current_epoch_numerator,
            42 / 2 * 3 + (81 + 42) / 2
        );
        assert!(!price_data.time_machine.current_epoch_is_valid);
        assert_eq!(price_data.time_machine.current_epoch_denominator, 3 + 1);
    }

    // next price doesnt change and neither does aggregate but slot does
    populate_instruction(&mut instruction_data, 81, 2, 4);
    update_clock_slot(&mut clock_account, 5);
    assert!(process_instruction(
        &program_id,
        &[
            funding_account.clone(),
            price_account.clone(),
            clock_account.clone()
        ],
        &instruction_data
    )
    .is_ok());

    {
        let price_data = load_checked::<PriceAccountWrapper>(&price_account, PC_VERSION).unwrap();
        assert_eq!(price_data.price_data.comp_[0].latest_.price_, 81);
        assert_eq!(price_data.price_data.comp_[0].latest_.conf_, 2);
        assert_eq!(price_data.price_data.comp_[0].latest_.pub_slot_, 4);
        assert_eq!(
            price_data.price_data.comp_[0].latest_.status_,
            PC_STATUS_TRADING
        );
        assert_eq!(price_data.price_data.valid_slot_, 4);
        assert_eq!(price_data.price_data.agg_.pub_slot_, 5);
        assert_eq!(price_data.price_data.agg_.price_, 81);
        assert_eq!(price_data.price_data.agg_.status_, PC_STATUS_TRADING);

        assert_eq!(
            price_data.time_machine.current_epoch_numerator,
            42 / 2 * 3 + (81 + 42) / 2 + 81
        );
        assert!(!price_data.time_machine.current_epoch_is_valid);
        assert_eq!(price_data.time_machine.current_epoch_denominator, 3 + 1 + 1);
    }

    // try to publish back-in-time
    populate_instruction(&mut instruction_data, 81, 2, 1);
    update_clock_slot(&mut clock_account, 5);
    assert_eq!(
        process_instruction(
            &program_id,
            &[
                funding_account.clone(),
                price_account.clone(),
                clock_account.clone()
            ],
            &instruction_data
        ),
        Err(ProgramError::InvalidArgument)
    );

    {
        let price_data = load_checked::<PriceAccountWrapper>(&price_account, PC_VERSION).unwrap();
        assert_eq!(price_data.price_data.comp_[0].latest_.price_, 81);
        assert_eq!(price_data.price_data.comp_[0].latest_.conf_, 2);
        assert_eq!(price_data.price_data.comp_[0].latest_.pub_slot_, 4);
        assert_eq!(
            price_data.price_data.comp_[0].latest_.status_,
            PC_STATUS_TRADING
        );
        assert_eq!(price_data.price_data.valid_slot_, 4);
        assert_eq!(price_data.price_data.agg_.pub_slot_, 5);
        assert_eq!(price_data.price_data.agg_.price_, 81);
        assert_eq!(price_data.price_data.agg_.status_, PC_STATUS_TRADING);

        assert_eq!(
            price_data.time_machine.current_epoch_numerator,
            42 / 2 * 3 + (81 + 42) / 2 + 81
        );
        assert!(!price_data.time_machine.current_epoch_is_valid);
        assert_eq!(price_data.time_machine.current_epoch_denominator, 3 + 1 + 1);
    }

    populate_instruction(&mut instruction_data, 50, 20, 5);
    update_clock_slot(&mut clock_account, 6);

    // Publishing a wide CI results in a status of unknown.

    // check that someone doesn't accidentally break the test.
    {
        let price_data = load_checked::<PriceAccountWrapper>(&price_account, PC_VERSION).unwrap();
        assert_eq!(
            price_data.price_data.comp_[0].latest_.status_,
            PC_STATUS_TRADING
        );
    }

    assert!(process_instruction(
        &program_id,
        &[
            funding_account.clone(),
            price_account.clone(),
            clock_account.clone()
        ],
        &instruction_data
    )
    .is_ok());

    {
        let price_data = load_checked::<PriceAccountWrapper>(&price_account, PC_VERSION).unwrap();
        assert_eq!(price_data.price_data.comp_[0].latest_.price_, 50);
        assert_eq!(price_data.price_data.comp_[0].latest_.conf_, 20);
        assert_eq!(price_data.price_data.comp_[0].latest_.pub_slot_, 5);
        assert_eq!(
            price_data.price_data.comp_[0].latest_.status_,
            PC_STATUS_UNKNOWN
        );
        assert_eq!(price_data.price_data.valid_slot_, 5);
        assert_eq!(price_data.price_data.agg_.pub_slot_, 6);
        assert_eq!(price_data.price_data.agg_.price_, 81);
        assert_eq!(price_data.price_data.agg_.status_, PC_STATUS_TRADING);

        assert_eq!(
            price_data.time_machine.current_epoch_numerator,
            42 / 2 * 3 + (81 + 42) / 2 + 81 + 81
        );
        assert!(!price_data.time_machine.current_epoch_is_valid);
        assert_eq!(
            price_data.time_machine.current_epoch_denominator,
            3 + 1 + 1 + 1
        );
    }

    // Crank one more time and aggregate should be unknown
    populate_instruction(&mut instruction_data, 50, 20, 6);
    update_clock_slot(&mut clock_account, 7);

    assert!(process_instruction(
        &program_id,
        &[
            funding_account.clone(),
            price_account.clone(),
            clock_account.clone()
        ],
        &instruction_data
    )
    .is_ok());

    {
        let price_data = load_checked::<PriceAccountWrapper>(&price_account, PC_VERSION).unwrap();
        assert_eq!(price_data.price_data.comp_[0].latest_.price_, 50);
        assert_eq!(price_data.price_data.comp_[0].latest_.conf_, 20);
        assert_eq!(price_data.price_data.comp_[0].latest_.pub_slot_, 6);
        assert_eq!(
            price_data.price_data.comp_[0].latest_.status_,
            PC_STATUS_UNKNOWN
        );
        assert_eq!(price_data.price_data.valid_slot_, 6);
        assert_eq!(price_data.price_data.agg_.pub_slot_, 7);
        assert_eq!(price_data.price_data.agg_.price_, 81);
        assert_eq!(price_data.price_data.agg_.status_, PC_STATUS_UNKNOWN);

        assert_eq!(
            price_data.time_machine.current_epoch_numerator,
            42 / 2 * 3 + (81 + 42) / 2 + 81 + 81
        );
        assert!(!price_data.time_machine.current_epoch_is_valid);
        assert_eq!(
            price_data.time_machine.current_epoch_denominator,
            3 + 1 + 1 + 1
        );
    }
}

// Create an upd_price instruction with the provided parameters
fn populate_instruction(instruction_data: &mut [u8], price: i64, conf: u64, pub_slot: u64) {
    let mut cmd = load_mut::<UpdPriceArgs>(instruction_data).unwrap();
    cmd.header = OracleCommand::UpdPrice.into();
    cmd.status = PC_STATUS_TRADING;
    cmd.price = price;
    cmd.confidence = conf;
    cmd.publishing_slot = pub_slot;
    cmd.unused_ = 0;
}
