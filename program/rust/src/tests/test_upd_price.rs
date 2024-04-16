use {
    crate::{
        accounts::{
            PriceAccount,
            PythAccount,
        },
        c_oracle_header::{
            PC_STATUS_IGNORED,
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
        processor::process_instruction,
        tests::test_utils::{
            update_clock_slot,
            update_clock_timestamp,
            AccountSetup,
        },
    },
    solana_program::{
        program_error::ProgramError,
        pubkey::Pubkey,
    },
    solana_sdk::account_info::AccountInfo,
    std::mem::size_of,
};

#[test]
fn test_upd_price() {
    let mut instruction_data = [0u8; size_of::<UpdPriceArgs>()];
    populate_instruction(&mut instruction_data, 42, 2, 1);

    let program_id = Pubkey::new_unique();

    let mut funding_setup = AccountSetup::new_funding();
    let funding_account = funding_setup.as_account_info();

    let mut price_setup = AccountSetup::new::<PriceAccount>(&program_id);
    let mut price_account = price_setup.as_account_info();
    price_account.is_signer = false;
    PriceAccount::initialize(&price_account, PC_VERSION).unwrap();

    {
        let mut price_data = load_checked::<PriceAccount>(&price_account, PC_VERSION).unwrap();
        price_data.num_ = 1;
        price_data.comp_[0].pub_ = *funding_account.key;
    }

    let mut clock_setup = AccountSetup::new_clock();
    let mut clock_account = clock_setup.as_account_info();
    clock_account.is_signer = false;
    clock_account.is_writable = false;

    update_clock_slot(&mut clock_account, 1);
    update_clock_timestamp(&mut clock_account, 1);

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
        let price_data = load_checked::<PriceAccount>(&price_account, PC_VERSION).unwrap();
        assert_eq!(price_data.comp_[0].latest_.price_, 42);
        assert_eq!(price_data.comp_[0].latest_.conf_, 2);
        assert_eq!(price_data.comp_[0].latest_.pub_slot_, 1);
        assert_eq!(price_data.comp_[0].latest_.status_, PC_STATUS_TRADING);
        assert_eq!(price_data.valid_slot_, 0);
        assert_eq!(price_data.last_slot_, 1);
        assert_eq!(price_data.agg_.pub_slot_, 1);
        assert_eq!(price_data.agg_.price_, 42);
        assert_eq!(price_data.agg_.status_, PC_STATUS_TRADING);
        assert_eq!(price_data.prev_slot_, 0);
        assert_eq!(price_data.prev_price_, 0);
        assert_eq!(price_data.prev_conf_, 0);
        assert_eq!(price_data.prev_timestamp_, 0);
        assert_eq!(price_data.price_cumulative.price, 42);
        assert_eq!(price_data.price_cumulative.conf, 2);
        assert_eq!(price_data.price_cumulative.num_down_slots, 0);
    }

    // a publisher's component pub_slot_ has to be strictly increasing -- get rejected
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
        let price_data = load_checked::<PriceAccount>(&price_account, PC_VERSION).unwrap();
        assert_eq!(price_data.comp_[0].latest_.price_, 42);
        assert_eq!(price_data.comp_[0].latest_.conf_, 2);
        assert_eq!(price_data.comp_[0].latest_.pub_slot_, 1);
        assert_eq!(price_data.comp_[0].latest_.status_, PC_STATUS_TRADING);
        assert_eq!(price_data.valid_slot_, 0);
        assert_eq!(price_data.last_slot_, 1);
        assert_eq!(price_data.agg_.pub_slot_, 1);
        assert_eq!(price_data.agg_.price_, 42);
        assert_eq!(price_data.agg_.status_, PC_STATUS_TRADING);
        assert_eq!(price_data.prev_slot_, 0);
        assert_eq!(price_data.prev_price_, 0);
        assert_eq!(price_data.prev_conf_, 0);
        assert_eq!(price_data.prev_timestamp_, 0);
        assert_eq!(price_data.price_cumulative.price, 42);
        assert_eq!(price_data.price_cumulative.conf, 2);
        assert_eq!(price_data.price_cumulative.num_down_slots, 0);
    }

    // update new price in new slot, aggregate should be updated and prev values should be updated
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
        let price_data = load_checked::<PriceAccount>(&price_account, PC_VERSION).unwrap();
        assert_eq!(price_data.comp_[0].latest_.price_, 81);
        assert_eq!(price_data.comp_[0].latest_.conf_, 2);
        assert_eq!(price_data.comp_[0].latest_.pub_slot_, 2);
        assert_eq!(price_data.comp_[0].latest_.status_, PC_STATUS_TRADING);
        assert_eq!(price_data.valid_slot_, 1);
        assert_eq!(price_data.last_slot_, 3);
        assert_eq!(price_data.agg_.pub_slot_, 3);
        assert_eq!(price_data.agg_.price_, 81);
        assert_eq!(price_data.agg_.status_, PC_STATUS_TRADING);
        assert_eq!(price_data.prev_slot_, 1);
        assert_eq!(price_data.prev_price_, 42);
        assert_eq!(price_data.prev_conf_, 2);
        assert_eq!(price_data.prev_timestamp_, 1);
        assert_eq!(price_data.price_cumulative.price, 42 + 2 * 81);
        assert_eq!(price_data.price_cumulative.conf, 3 * 2);
        assert_eq!(price_data.price_cumulative.num_down_slots, 0);
    }

    // next price doesn't change but slot and timestamp does
    populate_instruction(&mut instruction_data, 81, 2, 3);
    update_clock_slot(&mut clock_account, 4);
    update_clock_timestamp(&mut clock_account, 4);
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
        let price_data = load_checked::<PriceAccount>(&price_account, PC_VERSION).unwrap();
        assert_eq!(price_data.comp_[0].latest_.price_, 81);
        assert_eq!(price_data.comp_[0].latest_.conf_, 2);
        assert_eq!(price_data.comp_[0].latest_.pub_slot_, 3);
        assert_eq!(price_data.comp_[0].latest_.status_, PC_STATUS_TRADING);
        assert_eq!(price_data.valid_slot_, 3);
        assert_eq!(price_data.last_slot_, 4);
        assert_eq!(price_data.agg_.pub_slot_, 4);
        assert_eq!(price_data.agg_.price_, 81);
        assert_eq!(price_data.agg_.status_, PC_STATUS_TRADING);
        assert_eq!(price_data.prev_slot_, 3);
        assert_eq!(price_data.prev_price_, 81);
        assert_eq!(price_data.prev_conf_, 2);
        assert_eq!(price_data.timestamp_, 4); // We only check for timestamp_ here because test_upd_price doesn't directly update timestamp_, this is updated through c_upd_aggregate which is tested in test_upd_aggregate, but we assert here to show that in subsequent asserts for prev_timestamp_ the value should be updated to this value
        assert_eq!(price_data.prev_timestamp_, 1);
        assert_eq!(price_data.price_cumulative.price, 42 + 3 * 81);
        assert_eq!(price_data.price_cumulative.conf, 4 * 2);
        assert_eq!(price_data.price_cumulative.num_down_slots, 0);
    }

    // next price doesn't change and neither does aggregate but slot does
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
        let price_data = load_checked::<PriceAccount>(&price_account, PC_VERSION).unwrap();
        assert_eq!(price_data.comp_[0].latest_.price_, 81);
        assert_eq!(price_data.comp_[0].latest_.conf_, 2);
        assert_eq!(price_data.comp_[0].latest_.pub_slot_, 4);
        assert_eq!(price_data.comp_[0].latest_.status_, PC_STATUS_TRADING);
        assert_eq!(price_data.valid_slot_, 4);
        assert_eq!(price_data.last_slot_, 5);
        assert_eq!(price_data.agg_.pub_slot_, 5);
        assert_eq!(price_data.agg_.price_, 81);
        assert_eq!(price_data.agg_.status_, PC_STATUS_TRADING);
        assert_eq!(price_data.prev_slot_, 4);
        assert_eq!(price_data.prev_price_, 81);
        assert_eq!(price_data.prev_conf_, 2);
        assert_eq!(price_data.prev_timestamp_, 4);
        assert_eq!(price_data.price_cumulative.price, 42 + 81 * 4);
        assert_eq!(price_data.price_cumulative.conf, 5 * 2);
        assert_eq!(price_data.price_cumulative.num_down_slots, 0);
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
        let price_data = load_checked::<PriceAccount>(&price_account, PC_VERSION).unwrap();
        assert_eq!(price_data.comp_[0].latest_.price_, 81);
        assert_eq!(price_data.comp_[0].latest_.conf_, 2);
        assert_eq!(price_data.comp_[0].latest_.pub_slot_, 4);
        assert_eq!(price_data.comp_[0].latest_.status_, PC_STATUS_TRADING);
        assert_eq!(price_data.valid_slot_, 4);
        assert_eq!(price_data.last_slot_, 5);
        assert_eq!(price_data.agg_.pub_slot_, 5);
        assert_eq!(price_data.agg_.price_, 81);
        assert_eq!(price_data.agg_.status_, PC_STATUS_TRADING);
        assert_eq!(price_data.prev_slot_, 4);
        assert_eq!(price_data.prev_price_, 81);
        assert_eq!(price_data.prev_conf_, 2);
        assert_eq!(price_data.prev_timestamp_, 4);
        assert_eq!(price_data.price_cumulative.price, 42 + 81 * 4);
        assert_eq!(price_data.price_cumulative.conf, 5 * 2);
        assert_eq!(price_data.price_cumulative.num_down_slots, 0);
    }

    populate_instruction(&mut instruction_data, 50, 20, 5);
    update_clock_slot(&mut clock_account, 6);

    // Publishing a wide CI results in a status of unknown.

    // check that someone doesn't accidentally break the test.
    {
        let price_data = load_checked::<PriceAccount>(&price_account, PC_VERSION).unwrap();
        assert_eq!(price_data.comp_[0].latest_.status_, PC_STATUS_TRADING);
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
        let price_data = load_checked::<PriceAccount>(&price_account, PC_VERSION).unwrap();
        assert_eq!(price_data.comp_[0].latest_.price_, 50);
        assert_eq!(price_data.comp_[0].latest_.conf_, 20);
        assert_eq!(price_data.comp_[0].latest_.pub_slot_, 5);
        assert_eq!(price_data.comp_[0].latest_.status_, PC_STATUS_IGNORED);
        assert_eq!(price_data.valid_slot_, 5);
        assert_eq!(price_data.last_slot_, 5);
        assert_eq!(price_data.agg_.pub_slot_, 6);
        assert_eq!(price_data.agg_.price_, 81);
        assert_eq!(price_data.agg_.status_, PC_STATUS_UNKNOWN);
        assert_eq!(price_data.prev_slot_, 5);
        assert_eq!(price_data.prev_price_, 81);
        assert_eq!(price_data.prev_conf_, 2);
        assert_eq!(price_data.prev_timestamp_, 4);
        assert_eq!(price_data.price_cumulative.price, 42 + 81 * 4);
        assert_eq!(price_data.price_cumulative.conf, 2 * 5);
        assert_eq!(price_data.price_cumulative.num_down_slots, 0);
    }

    // Negative prices are accepted
    populate_instruction(&mut instruction_data, -100, 1, 7);
    update_clock_slot(&mut clock_account, 8);

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
        let price_data = load_checked::<PriceAccount>(&price_account, PC_VERSION).unwrap();
        assert_eq!(price_data.comp_[0].latest_.price_, -100);
        assert_eq!(price_data.comp_[0].latest_.conf_, 1);
        assert_eq!(price_data.comp_[0].latest_.pub_slot_, 7);
        assert_eq!(price_data.comp_[0].latest_.status_, PC_STATUS_TRADING);
        assert_eq!(price_data.valid_slot_, 6);
        assert_eq!(price_data.last_slot_, 8);
        assert_eq!(price_data.agg_.pub_slot_, 8);
        assert_eq!(price_data.agg_.price_, -100);
        assert_eq!(price_data.agg_.status_, PC_STATUS_TRADING);
        assert_eq!(price_data.prev_slot_, 5);
        assert_eq!(price_data.prev_price_, 81);
        assert_eq!(price_data.prev_conf_, 2);
        assert_eq!(price_data.prev_timestamp_, 4);
        assert_eq!(price_data.price_cumulative.price, 42 + 81 * 4 - 100 * 3);
        assert_eq!(price_data.price_cumulative.conf, 2 * 5 + 3); // 2 * 5 + 1 * 3
        assert_eq!(price_data.price_cumulative.num_down_slots, 0);
    }

    // add new test for multiple publishers and ensure that agg price is not updated multiple times when program upgrade happens in the same slot after the first update
    let mut funding_setup_two = AccountSetup::new_funding();
    let funding_account_two = funding_setup_two.as_account_info();

    add_publisher(&mut price_account, funding_account_two.key);

    populate_instruction(&mut instruction_data, 10, 1, 10);
    update_clock_slot(&mut clock_account, 10);

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
        let price_data = load_checked::<PriceAccount>(&price_account, PC_VERSION).unwrap();
        assert_eq!(price_data.comp_[0].latest_.price_, 10);
        assert_eq!(price_data.comp_[0].latest_.conf_, 1);
        assert_eq!(price_data.comp_[0].latest_.pub_slot_, 10);
        assert_eq!(price_data.comp_[0].latest_.status_, PC_STATUS_TRADING);
        assert_eq!(price_data.valid_slot_, 8);
        assert_eq!(price_data.last_slot_, 10);
        assert_eq!(price_data.agg_.pub_slot_, 10);
        assert_eq!(price_data.agg_.price_, 10);
        assert_eq!(price_data.agg_.status_, PC_STATUS_TRADING);
        assert_eq!(price_data.prev_slot_, 8);
        assert_eq!(price_data.prev_price_, -100);
        assert_eq!(price_data.prev_conf_, 1);
        assert_eq!(price_data.prev_timestamp_, 4);
        assert_eq!(price_data.twap_.numer_, 1677311098);
        assert_eq!(price_data.twap_.denom_, 1279419481);
        assert_eq!(
            price_data.price_cumulative.price,
            42 + 81 * 4 - 100 * 3 + 10 * 2
        );
        assert_eq!(price_data.price_cumulative.conf, 2 * 5 + 5); // 2 * 5 + 1 * 5
        assert_eq!(price_data.price_cumulative.num_down_slots, 0);
    }

    // reset twap_.denom_ to 0 to simulate program upgrade in the same slot and make sure agg_.price_ is not updated again
    {
        let mut price_data = load_checked::<PriceAccount>(&price_account, PC_VERSION).unwrap();
        price_data.prev_twap_.denom_ = 0;
    }
    populate_instruction(&mut instruction_data, 20, 1, 10);
    assert!(process_instruction(
        &program_id,
        &[
            funding_account_two.clone(),
            price_account.clone(),
            clock_account.clone()
        ],
        &instruction_data
    )
    .is_ok());

    {
        let price_data = load_checked::<PriceAccount>(&price_account, PC_VERSION).unwrap();
        assert_eq!(price_data.comp_[1].latest_.price_, 20);
        assert_eq!(price_data.comp_[1].latest_.conf_, 1);
        assert_eq!(price_data.comp_[1].latest_.pub_slot_, 10);
        assert_eq!(price_data.comp_[1].latest_.status_, PC_STATUS_TRADING);
        assert_eq!(price_data.valid_slot_, 8);
        assert_eq!(price_data.last_slot_, 10);
        assert_eq!(price_data.agg_.pub_slot_, 10);
        assert_eq!(price_data.agg_.price_, 10);
        assert_eq!(price_data.agg_.status_, PC_STATUS_TRADING);
        assert_eq!(price_data.prev_slot_, 8);
        assert_eq!(price_data.prev_price_, -100);
        assert_eq!(price_data.prev_conf_, 1);
        assert_eq!(price_data.prev_timestamp_, 4);
        assert_eq!(price_data.twap_.numer_, 1677311098); // twap_.numer_ should not be updated
        assert_eq!(price_data.twap_.denom_, 1279419481); // twap_.denom_ should not be updated

        // price_cumulative should not be updated
        assert_eq!(
            price_data.price_cumulative.price,
            42 + 81 * 4 - 100 * 3 + 10 * 2
        );
        assert_eq!(price_data.price_cumulative.conf, 2 * 5 + 5);
        assert_eq!(price_data.price_cumulative.num_down_slots, 0);
    }

    remove_publisher(&mut price_account);
    // Big gap
    populate_instruction(&mut instruction_data, 60, 4, 50);
    update_clock_slot(&mut clock_account, 50);

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
        let price_data = load_checked::<PriceAccount>(&price_account, PC_VERSION).unwrap();
        assert_eq!(price_data.comp_[0].latest_.price_, 60);
        assert_eq!(price_data.comp_[0].latest_.conf_, 4);
        assert_eq!(price_data.comp_[0].latest_.pub_slot_, 50);
        assert_eq!(price_data.comp_[0].latest_.status_, PC_STATUS_TRADING);
        assert_eq!(price_data.valid_slot_, 10);
        assert_eq!(price_data.agg_.pub_slot_, 50);
        assert_eq!(price_data.agg_.price_, 60);
        assert_eq!(price_data.agg_.conf_, 4);
        assert_eq!(price_data.agg_.status_, PC_STATUS_TRADING);
        assert_eq!(
            price_data.price_cumulative.price,
            42 + 81 * 4 - 100 * 3 + 10 * 2 + 60 * 40
        );
        assert_eq!(price_data.price_cumulative.conf, 2 * 5 + 5 + 40 * 4);
        assert_eq!(price_data.price_cumulative.num_down_slots, 15);
    }

    // add new test for multiple publishers and ensure that price_cumulative is updated correctly
    add_publisher(&mut price_account, funding_account_two.key);
    populate_instruction(&mut instruction_data, 10, 1, 100);
    update_clock_slot(&mut clock_account, 100);
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
        let price_data = load_checked::<PriceAccount>(&price_account, PC_VERSION).unwrap();
        assert_eq!(price_data.comp_[0].latest_.price_, 10);
        assert_eq!(price_data.comp_[0].latest_.conf_, 1);
        assert_eq!(price_data.comp_[0].latest_.pub_slot_, 100);
        assert_eq!(price_data.comp_[0].latest_.status_, PC_STATUS_TRADING);
        assert_eq!(price_data.valid_slot_, 50);
        assert_eq!(price_data.agg_.pub_slot_, 100);
        assert_eq!(price_data.agg_.price_, 10);
        assert_eq!(price_data.agg_.conf_, 1);
        assert_eq!(price_data.agg_.status_, PC_STATUS_TRADING);
        assert_eq!(price_data.prev_slot_, 50);
        assert_eq!(price_data.prev_price_, 60);
        assert_eq!(price_data.prev_conf_, 4);
        assert_eq!(
            price_data.prev_price_cumulative.price,
            42 + 81 * 4 - 100 * 3 + 10 * 2 + 60 * 40
        );
        assert_eq!(price_data.prev_price_cumulative.conf, 2 * 5 + 5 + 40 * 4);
        assert_eq!(price_data.prev_price_cumulative.num_down_slots, 15);
        assert_eq!(
            price_data.price_cumulative.price,
            42 + 81 * 4 - 100 * 3 + 10 * 2 + 60 * 40 + 10 * 50
        ); // (42 + 81 * 4 - 100 * 3 + 10 * 2 + 60 * 40) is the price cumulative from the previous test and 10 * 50 (slot_gap) is the price cumulative from this test
        assert_eq!(price_data.price_cumulative.conf, 2 * 5 + 5 + 40 * 4 + 50);
        assert_eq!(price_data.price_cumulative.num_down_slots, 40); // prev num_down_slots was 15 and since pub slot is 100 and last pub slot was 50, slot_gap is 50 and default latency is 25, so num_down_slots = 50 - 25 = 25, so total num_down_slots = 15 + 25 = 40
    }

    populate_instruction(&mut instruction_data, 20, 2, 100);
    assert!(process_instruction(
        &program_id,
        &[
            funding_account_two.clone(),
            price_account.clone(),
            clock_account.clone()
        ],
        &instruction_data
    )
    .is_ok());

    {
        let price_data = load_checked::<PriceAccount>(&price_account, PC_VERSION).unwrap();
        assert_eq!(price_data.comp_[0].latest_.price_, 10);
        assert_eq!(price_data.comp_[0].latest_.conf_, 1);
        assert_eq!(price_data.comp_[0].latest_.pub_slot_, 100);
        assert_eq!(price_data.comp_[0].latest_.status_, PC_STATUS_TRADING);
        assert_eq!(price_data.comp_[1].latest_.price_, 20);
        assert_eq!(price_data.comp_[1].latest_.conf_, 2);
        assert_eq!(price_data.comp_[1].latest_.pub_slot_, 100);
        assert_eq!(price_data.comp_[1].latest_.status_, PC_STATUS_TRADING);
        assert_eq!(price_data.valid_slot_, 100);
        assert_eq!(price_data.agg_.pub_slot_, 100);
        assert_eq!(price_data.agg_.price_, 14);
        assert_eq!(price_data.agg_.conf_, 6);
        assert_eq!(price_data.agg_.status_, PC_STATUS_TRADING);
        assert_eq!(price_data.prev_slot_, 50);
        assert_eq!(price_data.prev_price_, 60);
        assert_eq!(price_data.prev_conf_, 4);
        assert_eq!(
            price_data.prev_price_cumulative.price,
            42 + 81 * 4 - 100 * 3 + 10 * 2 + 60 * 40
        );
        assert_eq!(price_data.prev_price_cumulative.conf, 2 * 5 + 5 + 40 * 4);
        assert_eq!(price_data.prev_price_cumulative.num_down_slots, 15);
        assert_eq!(
            price_data.price_cumulative.price,
            42 + 81 * 4 - 100 * 3 + 10 * 2 + 60 * 40 + 14 * 50
        ); // (42 + 81 * 4 - 100 * 3 + 10 * 2 + 60 * 40) is the price cumulative from the previous test and 14 * 50 (slot_gap) is the price cumulative from this test
        assert_eq!(
            price_data.price_cumulative.conf,
            2 * 5 + 5 + 40 * 4 + 6 * 50
        );
        assert_eq!(price_data.price_cumulative.num_down_slots, 40); // prev num_down_slots was 15 and since pub slot is 100 and last pub slot was 50, slot_gap is 50 and default latency is 25, so num_down_slots = 50 - 25 = 25, so total num_down_slots = 15 + 25 = 40
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

fn add_publisher(price_account: &mut AccountInfo, publisher_key: &Pubkey) {
    let mut price_data = load_checked::<PriceAccount>(price_account, PC_VERSION).unwrap();
    let index = price_data.num_ as usize;
    price_data.comp_[index].pub_ = *publisher_key;
    price_data.num_ += 1;
}

fn remove_publisher(price_account: &mut AccountInfo) {
    let mut price_data = load_checked::<PriceAccount>(price_account, PC_VERSION).unwrap();
    if price_data.num_ > 0 {
        price_data.num_ -= 1;
    }
}
