use solana_program::pubkey::Pubkey;
use std::mem::size_of;

use crate::c_oracle_header::{
    PC_MAX_SEND_LATENCY,
    PC_STATUS_TRADING,
    PC_VERSION,
};

use crate::deserialize::{
    initialize_pyth_account_checked,
    load_checked,
    load_mut,
};
use crate::instruction::{
    OracleCommand,
    UpdPriceArgs,
};
use crate::processor::process_instruction;
use crate::tests::test_utils::{
    update_clock_slot,
    update_clock_timestamp,
    AccountSetup,
};
use crate::time_machine_types::{
    PriceAccountWrapper,
    NUM_BUCKETS_THIRTY_MIN,
    THIRTY_MINUTES,
};

/// Manually test some epoch transitions
#[test]
fn test_sma_epoch_transition() {
    let mut instruction_data = [0u8; size_of::<UpdPriceArgs>()];

    let program_id = Pubkey::new_unique();

    let mut funding_setup = AccountSetup::new_funding();
    let funding_account = funding_setup.to_account_info();

    let mut price_setup = AccountSetup::new::<PriceAccountWrapper>(&program_id);
    let mut price_account = price_setup.to_account_info();
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
    let mut clock_account = clock_setup.to_account_info();
    clock_account.is_signer = false;
    clock_account.is_writable = false;

    update_clock_slot(&mut clock_account, 1);
    update_clock_timestamp(&mut clock_account, 1);
    populate_instruction(&mut instruction_data, 42, 2, 1);

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

        assert_eq!(
            price_data.time_machine.threshold,
            PC_MAX_SEND_LATENCY as u64
        );
        assert_eq!(price_data.time_machine.granularity, THIRTY_MINUTES);
        assert_eq!(price_data.time_machine.current_epoch_numerator, 0);
        assert_eq!(price_data.time_machine.current_epoch_is_valid, true);
        assert_eq!(price_data.time_machine.current_epoch_denominator, 0);
        for i in 0..NUM_BUCKETS_THIRTY_MIN {
            assert_eq!(price_data.time_machine.running_sum_of_price_averages[i], 0);
            assert_eq!(price_data.time_machine.running_valid_epoch_counter[i], 0);
        }
    }

    // Same epoch, small slot gap
    update_clock_slot(&mut clock_account, 2);
    update_clock_timestamp(&mut clock_account, 2);
    populate_instruction(&mut instruction_data, 80, 2, 2);

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

        assert_eq!(
            price_data.time_machine.threshold,
            PC_MAX_SEND_LATENCY as u64
        );
        assert_eq!(price_data.time_machine.granularity, THIRTY_MINUTES);
        assert_eq!(price_data.time_machine.current_epoch_numerator, 42 / 2 * 2);
        assert_eq!(price_data.time_machine.current_epoch_is_valid, true);
        assert_eq!(price_data.time_machine.current_epoch_denominator, 2);
        for i in 0..NUM_BUCKETS_THIRTY_MIN {
            assert_eq!(price_data.time_machine.running_sum_of_price_averages[i], 0);
            assert_eq!(price_data.time_machine.running_valid_epoch_counter[i], 0);
        }
    }

    // Next epoch, small slot gap
    update_clock_slot(&mut clock_account, 3);
    update_clock_timestamp(&mut clock_account, THIRTY_MINUTES);
    populate_instruction(&mut instruction_data, 120, 1, 3);

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

        assert_eq!(
            price_data.time_machine.threshold,
            PC_MAX_SEND_LATENCY as u64
        );
        assert_eq!(price_data.time_machine.granularity, THIRTY_MINUTES);
        assert_eq!(
            price_data.time_machine.current_epoch_numerator,
            (80 + 42) / 2
        );
        assert_eq!(price_data.time_machine.current_epoch_is_valid, true);
        assert_eq!(price_data.time_machine.current_epoch_denominator, 1);

        for i in 1..NUM_BUCKETS_THIRTY_MIN {
            assert_eq!(price_data.time_machine.running_sum_of_price_averages[i], 0);
            assert_eq!(price_data.time_machine.running_valid_epoch_counter[i], 0);
        }

        assert_eq!(
            price_data.time_machine.running_sum_of_price_averages[0],
            ((42 + (80 + 42) / 2) / 3)
        );
        assert_eq!(price_data.time_machine.running_valid_epoch_counter[0], 1);
    }

    // Same epoch, big slot gap
    update_clock_slot(&mut clock_account, 30);
    update_clock_timestamp(&mut clock_account, THIRTY_MINUTES + 1);
    populate_instruction(&mut instruction_data, 40, 1, 30);

    // Unsuccesful aggregation
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

        assert_eq!(
            price_data.time_machine.threshold,
            PC_MAX_SEND_LATENCY as u64
        );
        assert_eq!(price_data.time_machine.granularity, THIRTY_MINUTES);
        assert_eq!(
            price_data.time_machine.current_epoch_numerator,
            (80 + 42) / 2
        );
        assert_eq!(price_data.time_machine.current_epoch_is_valid, true);
        assert_eq!(price_data.time_machine.current_epoch_denominator, 1);

        for i in 1..NUM_BUCKETS_THIRTY_MIN {
            assert_eq!(price_data.time_machine.running_sum_of_price_averages[i], 0);
            assert_eq!(price_data.time_machine.running_valid_epoch_counter[i], 0);
        }

        assert_eq!(
            price_data.time_machine.running_sum_of_price_averages[0],
            ((42 + (80 + 42) / 2) / 3)
        );
        assert_eq!(price_data.time_machine.running_valid_epoch_counter[0], 1);
    }

    update_clock_slot(&mut clock_account, 31);
    update_clock_timestamp(&mut clock_account, 2 * THIRTY_MINUTES + 1);
    populate_instruction(&mut instruction_data, 41, 1, 31);

    // Triggers aggregation!
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

        assert_eq!(
            price_data.time_machine.threshold,
            PC_MAX_SEND_LATENCY as u64
        );
        assert_eq!(price_data.time_machine.granularity, THIRTY_MINUTES);
        assert_eq!(
            price_data.time_machine.current_epoch_numerator,
            (40 + 80) / 2 * 28
        );
        assert_eq!(price_data.time_machine.current_epoch_is_valid, false);
        assert_eq!(price_data.time_machine.current_epoch_denominator, 28);

        for i in 2..NUM_BUCKETS_THIRTY_MIN {
            assert_eq!(price_data.time_machine.running_sum_of_price_averages[i], 0);
            assert_eq!(price_data.time_machine.running_valid_epoch_counter[i], 0);
        }

        assert_eq!(
            price_data.time_machine.running_sum_of_price_averages[0],
            ((42 + (80 + 42) / 2) / 3)
        );
        assert_eq!(price_data.time_machine.running_valid_epoch_counter[0], 1);

        assert_eq!(
            price_data.time_machine.running_sum_of_price_averages[1],
            price_data.time_machine.running_sum_of_price_averages[0] + 60
        );
        assert_eq!(price_data.time_machine.running_valid_epoch_counter[1], 1);
    }

    // A big gap in time (more than 1 epoch) but not in slots, all skipped epochs are "valid"
    update_clock_slot(&mut clock_account, 32);
    update_clock_timestamp(&mut clock_account, 5 * THIRTY_MINUTES + 1);
    populate_instruction(&mut instruction_data, 30, 1, 32);

    // Triggers aggregation!
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

        assert_eq!(
            price_data.time_machine.threshold,
            PC_MAX_SEND_LATENCY as u64
        );
        assert_eq!(price_data.time_machine.granularity, THIRTY_MINUTES);
        assert_eq!(
            price_data.time_machine.current_epoch_numerator,
            (40 + 41) / 2
        );
        assert_eq!(price_data.time_machine.current_epoch_is_valid, true);
        assert_eq!(price_data.time_machine.current_epoch_denominator, 1);

        for i in 5..NUM_BUCKETS_THIRTY_MIN {
            assert_eq!(price_data.time_machine.running_sum_of_price_averages[i], 0);
            assert_eq!(price_data.time_machine.running_valid_epoch_counter[i], 0);
        }

        assert_eq!(
            price_data.time_machine.running_sum_of_price_averages[0],
            ((42 + (80 + 42) / 2) / 3)
        );
        assert_eq!(price_data.time_machine.running_valid_epoch_counter[0], 1);

        assert_eq!(
            price_data.time_machine.running_sum_of_price_averages[1],
            price_data.time_machine.running_sum_of_price_averages[0] + 60
        );
        assert_eq!(price_data.time_machine.running_valid_epoch_counter[1], 1);

        assert_eq!(
            price_data.time_machine.running_sum_of_price_averages[2],
            price_data.time_machine.running_sum_of_price_averages[1] + (60 * 28 + 1 * 41) / 29
        );
        assert_eq!(price_data.time_machine.running_valid_epoch_counter[2], 1);

        assert_eq!(
            price_data.time_machine.running_sum_of_price_averages[3],
            price_data.time_machine.running_sum_of_price_averages[2] + 40
        );
        assert_eq!(price_data.time_machine.running_valid_epoch_counter[3], 2);

        assert_eq!(
            price_data.time_machine.running_sum_of_price_averages[4],
            price_data.time_machine.running_sum_of_price_averages[3] + 40
        );
        assert_eq!(price_data.time_machine.running_valid_epoch_counter[4], 3);
    }

    // Really big gap both in slots and epochs (the entire buffers gets rewritten)
    update_clock_slot(&mut clock_account, 100);
    update_clock_timestamp(&mut clock_account, 100 * THIRTY_MINUTES + 1);
    populate_instruction(&mut instruction_data, 100, 1, 100);

    // Unsuccesful aggregation
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

        assert_eq!(
            price_data.time_machine.threshold,
            PC_MAX_SEND_LATENCY as u64
        );
        assert_eq!(price_data.time_machine.granularity, THIRTY_MINUTES);
        assert_eq!(
            price_data.time_machine.current_epoch_numerator,
            (40 + 41) / 2
        );
        assert_eq!(price_data.time_machine.current_epoch_is_valid, true);
        assert_eq!(price_data.time_machine.current_epoch_denominator, 1);

        for i in 5..NUM_BUCKETS_THIRTY_MIN {
            assert_eq!(price_data.time_machine.running_sum_of_price_averages[i], 0);
            assert_eq!(price_data.time_machine.running_valid_epoch_counter[i], 0);
        }

        assert_eq!(
            price_data.time_machine.running_sum_of_price_averages[0],
            ((42 + (80 + 42) / 2) / 3)
        );
        assert_eq!(price_data.time_machine.running_valid_epoch_counter[0], 1);

        assert_eq!(
            price_data.time_machine.running_sum_of_price_averages[1],
            price_data.time_machine.running_sum_of_price_averages[0] + 60
        );
        assert_eq!(price_data.time_machine.running_valid_epoch_counter[1], 1);

        assert_eq!(
            price_data.time_machine.running_sum_of_price_averages[2],
            price_data.time_machine.running_sum_of_price_averages[1] + (60 * 28 + 1 * 41) / 29
        );
        assert_eq!(price_data.time_machine.running_valid_epoch_counter[2], 1);

        assert_eq!(
            price_data.time_machine.running_sum_of_price_averages[3],
            price_data.time_machine.running_sum_of_price_averages[2] + 40
        );
        assert_eq!(price_data.time_machine.running_valid_epoch_counter[3], 2);

        assert_eq!(
            price_data.time_machine.running_sum_of_price_averages[4],
            price_data.time_machine.running_sum_of_price_averages[3] + 40
        );
        assert_eq!(price_data.time_machine.running_valid_epoch_counter[4], 3);
    }

    update_clock_slot(&mut clock_account, 101);
    update_clock_timestamp(&mut clock_account, 100 * THIRTY_MINUTES + 2);
    populate_instruction(&mut instruction_data, 100, 1, 101);

    // Aggregation triggered
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

        assert_eq!(
            price_data.time_machine.threshold,
            PC_MAX_SEND_LATENCY as u64
        );
        assert_eq!(price_data.time_machine.granularity, THIRTY_MINUTES);
        assert_eq!(
            price_data.time_machine.current_epoch_numerator,
            (41 + 100) / 2 * 69
        );
        assert_eq!(price_data.time_machine.current_epoch_is_valid, false);
        assert_eq!(price_data.time_machine.current_epoch_denominator, 69);

        for i in 0..NUM_BUCKETS_THIRTY_MIN {
            assert_eq!(
                price_data.time_machine.running_sum_of_price_averages
                    [(i + 4) % NUM_BUCKETS_THIRTY_MIN],
                233 + 69 + 70 * (NUM_BUCKETS_THIRTY_MIN as i128 - 1) + 70 * i as i128
            );
            assert_eq!(
                price_data.time_machine.running_valid_epoch_counter
                    [(i + 4) % NUM_BUCKETS_THIRTY_MIN],
                3
            );
        }
    }
}

// Create an upd_price instruction with the provided parameters
fn populate_instruction(instruction_data: &mut [u8], price: i64, conf: u64, pub_slot: u64) -> () {
    let mut cmd = load_mut::<UpdPriceArgs>(instruction_data).unwrap();
    cmd.header = OracleCommand::UpdPrice.into();
    cmd.status = PC_STATUS_TRADING;
    cmd.price = price;
    cmd.confidence = conf;
    cmd.publishing_slot = pub_slot;
    cmd.unused_ = 0;
}
