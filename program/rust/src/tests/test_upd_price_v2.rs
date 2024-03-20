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
            AccountSetup,
        },
    },
    solana_program::{
        program_error::ProgramError,
        pubkey::Pubkey,
    },
    std::mem::size_of,
};

#[test]
fn test_upd_price_v2() -> Result<(), Box<dyn std::error::Error>> {
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

    process_instruction(
        &program_id,
        &[
            funding_account.clone(),
            price_account.clone(),
            clock_account.clone(),
        ],
        &instruction_data,
    )?;

    {
        let price_data = load_checked::<PriceAccount>(&price_account, PC_VERSION).unwrap();
        assert_eq!(price_data.comp_[0].latest_.price_, 42);
        assert_eq!(price_data.comp_[0].latest_.conf_, 2);
        assert_eq!(price_data.comp_[0].latest_.pub_slot_, 1);
        assert_eq!(price_data.comp_[0].latest_.status_, PC_STATUS_TRADING);
        assert_eq!(price_data.valid_slot_, 0);
        assert_eq!(price_data.agg_.pub_slot_, 1);
        assert_eq!(price_data.agg_.price_, 42);
        assert_eq!(price_data.agg_.conf_, 2);
        assert_eq!(price_data.agg_.status_, PC_STATUS_TRADING);
        assert_eq!(price_data.price_cumulative.price, 42);
        assert_eq!(price_data.price_cumulative.conf, 2);
        assert_eq!(price_data.price_cumulative.num_down_slots, 0);
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
        let price_data = load_checked::<PriceAccount>(&price_account, PC_VERSION).unwrap();
        assert_eq!(price_data.comp_[0].latest_.price_, 42);
        assert_eq!(price_data.comp_[0].latest_.conf_, 2);
        assert_eq!(price_data.comp_[0].latest_.pub_slot_, 1);
        assert_eq!(price_data.comp_[0].latest_.status_, PC_STATUS_TRADING);
        assert_eq!(price_data.valid_slot_, 0);
        assert_eq!(price_data.agg_.pub_slot_, 1);
        assert_eq!(price_data.agg_.price_, 42);
        assert_eq!(price_data.agg_.conf_, 2);
        assert_eq!(price_data.agg_.status_, PC_STATUS_TRADING);
        assert_eq!(price_data.price_cumulative.price, 42);
        assert_eq!(price_data.price_cumulative.conf, 2);
        assert_eq!(price_data.price_cumulative.num_down_slots, 0);
    }

    // add next price in new slot triggering snapshot and aggregate calc
    populate_instruction(&mut instruction_data, 81, 2, 2);
    update_clock_slot(&mut clock_account, 3);

    process_instruction(
        &program_id,
        &[
            funding_account.clone(),
            price_account.clone(),
            clock_account.clone(),
        ],
        &instruction_data,
    )?;

    {
        let price_data = load_checked::<PriceAccount>(&price_account, PC_VERSION).unwrap();
        assert_eq!(price_data.comp_[0].latest_.price_, 81);
        assert_eq!(price_data.comp_[0].latest_.conf_, 2);
        assert_eq!(price_data.comp_[0].latest_.pub_slot_, 2);
        assert_eq!(price_data.comp_[0].latest_.status_, PC_STATUS_TRADING);
        assert_eq!(price_data.valid_slot_, 1);
        assert_eq!(price_data.agg_.pub_slot_, 3);
        assert_eq!(price_data.agg_.price_, 81);
        assert_eq!(price_data.agg_.conf_, 2);
        assert_eq!(price_data.agg_.status_, PC_STATUS_TRADING);
        assert_eq!(price_data.price_cumulative.price, 42 + 2 * 81);
        assert_eq!(price_data.price_cumulative.conf, 3 * 2);
        assert_eq!(price_data.price_cumulative.num_down_slots, 0);
    }

    // next price doesn't change but slot does
    populate_instruction(&mut instruction_data, 81, 2, 3);
    update_clock_slot(&mut clock_account, 4);
    process_instruction(
        &program_id,
        &[
            funding_account.clone(),
            price_account.clone(),
            clock_account.clone(),
        ],
        &instruction_data,
    )?;

    {
        let price_data = load_checked::<PriceAccount>(&price_account, PC_VERSION).unwrap();
        assert_eq!(price_data.comp_[0].latest_.price_, 81);
        assert_eq!(price_data.comp_[0].latest_.conf_, 2);
        assert_eq!(price_data.comp_[0].latest_.pub_slot_, 3);
        assert_eq!(price_data.comp_[0].latest_.status_, PC_STATUS_TRADING);
        assert_eq!(price_data.valid_slot_, 3);
        assert_eq!(price_data.agg_.pub_slot_, 4);
        assert_eq!(price_data.agg_.price_, 81);
        assert_eq!(price_data.agg_.conf_, 2);
        assert_eq!(price_data.agg_.status_, PC_STATUS_TRADING);
        assert_eq!(price_data.price_cumulative.price, 42 + 3 * 81);
        assert_eq!(price_data.price_cumulative.conf, 4 * 2);
        assert_eq!(price_data.price_cumulative.num_down_slots, 0);
    }

    // next price doesn't change and neither does aggregate but slot does
    populate_instruction(&mut instruction_data, 81, 2, 4);
    update_clock_slot(&mut clock_account, 5);
    process_instruction(
        &program_id,
        &[
            funding_account.clone(),
            price_account.clone(),
            clock_account.clone(),
        ],
        &instruction_data,
    )?;

    {
        let price_data = load_checked::<PriceAccount>(&price_account, PC_VERSION).unwrap();
        assert_eq!(price_data.comp_[0].latest_.price_, 81);
        assert_eq!(price_data.comp_[0].latest_.conf_, 2);
        assert_eq!(price_data.comp_[0].latest_.pub_slot_, 4);
        assert_eq!(price_data.comp_[0].latest_.status_, PC_STATUS_TRADING);
        assert_eq!(price_data.valid_slot_, 4);
        assert_eq!(price_data.agg_.pub_slot_, 5);
        assert_eq!(price_data.agg_.price_, 81);
        assert_eq!(price_data.agg_.conf_, 2);
        assert_eq!(price_data.agg_.status_, PC_STATUS_TRADING);
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
        assert_eq!(price_data.agg_.pub_slot_, 5);
        assert_eq!(price_data.agg_.price_, 81);
        assert_eq!(price_data.agg_.conf_, 2);
        assert_eq!(price_data.agg_.status_, PC_STATUS_TRADING);
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

    process_instruction(
        &program_id,
        &[
            funding_account.clone(),
            price_account.clone(),
            clock_account.clone(),
        ],
        &instruction_data,
    )?;

    {
        let price_data = load_checked::<PriceAccount>(&price_account, PC_VERSION).unwrap();
        assert_eq!(price_data.comp_[0].latest_.price_, 50);
        assert_eq!(price_data.comp_[0].latest_.conf_, 20);
        assert_eq!(price_data.comp_[0].latest_.pub_slot_, 5);
        assert_eq!(price_data.comp_[0].latest_.status_, PC_STATUS_IGNORED);
        assert_eq!(price_data.valid_slot_, 5);
        assert_eq!(price_data.agg_.pub_slot_, 6);
        assert_eq!(price_data.agg_.price_, 81);
        assert_eq!(price_data.agg_.conf_, 2);
        assert_eq!(price_data.agg_.status_, PC_STATUS_UNKNOWN);
        assert_eq!(price_data.price_cumulative.price, 42 + 81 * 4);
        assert_eq!(price_data.price_cumulative.conf, 2 * 5);
        assert_eq!(price_data.price_cumulative.num_down_slots, 0);
    }

    // Crank one more time and aggregate should be unknown
    populate_instruction(&mut instruction_data, 50, 20, 6);
    update_clock_slot(&mut clock_account, 7);

    process_instruction(
        &program_id,
        &[
            funding_account.clone(),
            price_account.clone(),
            clock_account.clone(),
        ],
        &instruction_data,
    )?;

    {
        let price_data = load_checked::<PriceAccount>(&price_account, PC_VERSION).unwrap();
        assert_eq!(price_data.comp_[0].latest_.price_, 50);
        assert_eq!(price_data.comp_[0].latest_.conf_, 20);
        assert_eq!(price_data.comp_[0].latest_.pub_slot_, 6);
        assert_eq!(price_data.comp_[0].latest_.status_, PC_STATUS_IGNORED);
        assert_eq!(price_data.valid_slot_, 6);
        assert_eq!(price_data.agg_.pub_slot_, 7);
        assert_eq!(price_data.agg_.price_, 81);
        assert_eq!(price_data.agg_.conf_, 2);
        assert_eq!(price_data.agg_.status_, PC_STATUS_UNKNOWN);
        assert_eq!(price_data.price_cumulative.price, 42 + 81 * 4);
        assert_eq!(price_data.price_cumulative.conf, 2 * 5);
        assert_eq!(price_data.price_cumulative.num_down_slots, 0);
    }

    // Negative prices are accepted
    populate_instruction(&mut instruction_data, -100, 1, 7);
    update_clock_slot(&mut clock_account, 8);

    process_instruction(
        &program_id,
        &[
            funding_account.clone(),
            price_account.clone(),
            clock_account.clone(),
        ],
        &instruction_data,
    )?;

    {
        let price_data = load_checked::<PriceAccount>(&price_account, PC_VERSION).unwrap();
        assert_eq!(price_data.comp_[0].latest_.price_, -100);
        assert_eq!(price_data.comp_[0].latest_.conf_, 1);
        assert_eq!(price_data.comp_[0].latest_.pub_slot_, 7);
        assert_eq!(price_data.comp_[0].latest_.status_, PC_STATUS_TRADING);
        assert_eq!(price_data.valid_slot_, 7);
        assert_eq!(price_data.agg_.pub_slot_, 8);
        assert_eq!(price_data.agg_.price_, -100);
        assert_eq!(price_data.agg_.conf_, 1);
        assert_eq!(price_data.agg_.status_, PC_STATUS_TRADING);
        assert_eq!(price_data.price_cumulative.price, 42 + 81 * 4 - 100 * 3);
        assert_eq!(price_data.price_cumulative.conf, 2 * 5 + 3);
        assert_eq!(price_data.price_cumulative.num_down_slots, 0);
    }

    // Crank again for aggregate
    populate_instruction(&mut instruction_data, -100, 1, 8);
    update_clock_slot(&mut clock_account, 9);

    process_instruction(
        &program_id,
        &[
            funding_account.clone(),
            price_account.clone(),
            clock_account.clone(),
        ],
        &instruction_data,
    )?;

    {
        let price_data = load_checked::<PriceAccount>(&price_account, PC_VERSION).unwrap();
        assert_eq!(price_data.comp_[0].latest_.price_, -100);
        assert_eq!(price_data.comp_[0].latest_.conf_, 1);
        assert_eq!(price_data.comp_[0].latest_.pub_slot_, 8);
        assert_eq!(price_data.comp_[0].latest_.status_, PC_STATUS_TRADING);
        assert_eq!(price_data.valid_slot_, 8);
        assert_eq!(price_data.agg_.pub_slot_, 9);
        assert_eq!(price_data.agg_.price_, -100);
        assert_eq!(price_data.agg_.conf_, 1);
        assert_eq!(price_data.agg_.status_, PC_STATUS_TRADING);
        assert_eq!(price_data.price_cumulative.price, 42 + 81 * 4 - 100 * 4);
        assert_eq!(price_data.price_cumulative.conf, 2 * 5 + 4);
        assert_eq!(price_data.price_cumulative.num_down_slots, 0);
    }

    // Big gap

    populate_instruction(&mut instruction_data, 60, 4, 50);
    update_clock_slot(&mut clock_account, 50);

    process_instruction(
        &program_id,
        &[
            funding_account.clone(),
            price_account.clone(),
            clock_account.clone(),
        ],
        &instruction_data,
    )?;

    {
        let price_data = load_checked::<PriceAccount>(&price_account, PC_VERSION).unwrap();
        assert_eq!(price_data.comp_[0].latest_.price_, 60);
        assert_eq!(price_data.comp_[0].latest_.conf_, 4);
        assert_eq!(price_data.comp_[0].latest_.pub_slot_, 50);
        assert_eq!(price_data.comp_[0].latest_.status_, PC_STATUS_TRADING);
        assert_eq!(price_data.valid_slot_, 9);
        assert_eq!(price_data.agg_.pub_slot_, 50);
        assert_eq!(price_data.agg_.price_, 60);
        assert_eq!(price_data.agg_.conf_, 4);
        assert_eq!(price_data.agg_.status_, PC_STATUS_TRADING);
        assert_eq!(
            price_data.price_cumulative.price,
            42 + 81 * 4 - 100 * 4 + 60 * 41
        );
        assert_eq!(price_data.price_cumulative.conf, 2 * 5 + 4 + 41 * 4);
        assert_eq!(price_data.price_cumulative.num_down_slots, 16);
    }

    // Crank again for aggregate

    populate_instruction(&mut instruction_data, 55, 5, 51);
    update_clock_slot(&mut clock_account, 51);

    process_instruction(
        &program_id,
        &[
            funding_account.clone(),
            price_account.clone(),
            clock_account.clone(),
        ],
        &instruction_data,
    )?;

    {
        let price_data = load_checked::<PriceAccount>(&price_account, PC_VERSION).unwrap();
        assert_eq!(price_data.comp_[0].latest_.price_, 55);
        assert_eq!(price_data.comp_[0].latest_.conf_, 5);
        assert_eq!(price_data.comp_[0].latest_.pub_slot_, 51);
        assert_eq!(price_data.comp_[0].latest_.status_, PC_STATUS_TRADING);
        assert_eq!(price_data.valid_slot_, 50);
        assert_eq!(price_data.agg_.pub_slot_, 51);
        assert_eq!(price_data.agg_.price_, 55);
        assert_eq!(price_data.agg_.conf_, 5);
        assert_eq!(price_data.agg_.status_, PC_STATUS_TRADING);
        assert_eq!(
            price_data.price_cumulative.price,
            42 + 81 * 4 - 100 * 4 + 60 * 41 + 55
        );
        assert_eq!(price_data.price_cumulative.conf, 2 * 5 + 4 + 41 * 4 + 5);
        assert_eq!(price_data.price_cumulative.num_down_slots, 16);
    }

    let mut funding_setup_two = AccountSetup::new_funding();
    let funding_account_two = funding_setup_two.as_account_info();

    {
        let mut price_data = load_checked::<PriceAccount>(&price_account, PC_VERSION).unwrap();
        price_data.num_ = 2;
        price_data.comp_[1].pub_ = *funding_account_two.key;
    }

    populate_instruction(&mut instruction_data, 10, 1, 100);
    update_clock_slot(&mut clock_account, 100);
    process_instruction(
        &program_id,
        &[
            funding_account.clone(),
            price_account.clone(),
            clock_account.clone(),
        ],
        &instruction_data,
    )?;

    populate_instruction(&mut instruction_data, 20, 2, 100);
    process_instruction(
        &program_id,
        &[
            funding_account_two.clone(),
            price_account.clone(),
            clock_account.clone(),
        ],
        &instruction_data,
    )?;

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
        assert_eq!(
            price_data.price_cumulative.price,
            42 + 81 * 4 - 100 * 4 + 60 * 41 + 55 + 14 * 49
        );
        assert_eq!(
            price_data.price_cumulative.conf,
            2 * 5 + 4 + 41 * 4 + 5 + 6 * 49
        );
        assert_eq!(price_data.price_cumulative.num_down_slots, 40); // prev num_down_slots was 16 and since pub slot is 100 and last pub slot was 51, slot_gap is 49 and default latency is 25, so num_down_slots = 49 - 25 = 24, so total num_down_slots = 16 + 24 = 40
    }

    Ok(())
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
