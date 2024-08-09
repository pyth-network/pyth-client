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
        assert_eq!(price_data.agg_.price_, 0);
        assert_eq!(price_data.agg_.conf_, 0);
        assert_eq!(price_data.agg_.status_, PC_STATUS_UNKNOWN);

        assert_eq!(price_data.price_cumulative.price, 0);
        assert_eq!(price_data.price_cumulative.conf, 0);
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
        assert_eq!(price_data.agg_.price_, 0);
        assert_eq!(price_data.agg_.conf_, 0);
        assert_eq!(price_data.agg_.status_, PC_STATUS_UNKNOWN);

        assert_eq!(price_data.price_cumulative.price, 0);
        assert_eq!(price_data.price_cumulative.conf, 0);
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
        assert_eq!(price_data.agg_.price_, 42);
        assert_eq!(price_data.agg_.conf_, 2);
        assert_eq!(price_data.agg_.status_, PC_STATUS_TRADING);

        assert_eq!(price_data.price_cumulative.price, 3 * 42);
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

        assert_eq!(price_data.price_cumulative.price, 3 * 42 + 81);
        assert_eq!(price_data.price_cumulative.conf, 3 * 2 + 2);
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

        assert_eq!(price_data.price_cumulative.price, 3 * 42 + 81 * 2);
        assert_eq!(price_data.price_cumulative.conf, 3 * 2 + 2 * 2);
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

        assert_eq!(price_data.price_cumulative.price, 3 * 42 + 81 * 2);
        assert_eq!(price_data.price_cumulative.conf, 3 * 2 + 2 * 2);
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
        assert_eq!(price_data.agg_.status_, PC_STATUS_TRADING);

        assert_eq!(price_data.price_cumulative.price, 3 * 42 + 81 * 3);
        assert_eq!(price_data.price_cumulative.conf, 3 * 2 + 2 * 3);
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

        assert_eq!(price_data.price_cumulative.price, 3 * 42 + 81 * 3);
        assert_eq!(price_data.price_cumulative.conf, 3 * 2 + 2 * 3);
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
        assert_eq!(price_data.agg_.price_, 81);
        assert_eq!(price_data.agg_.conf_, 2);
        assert_eq!(price_data.agg_.status_, PC_STATUS_UNKNOWN);

        assert_eq!(price_data.price_cumulative.price, 3 * 42 + 81 * 3);
        assert_eq!(price_data.price_cumulative.conf, 3 * 2 + 2 * 3);
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

        assert_eq!(price_data.price_cumulative.price, 3 * 42 + 81 * 3 - 100 * 3);
        assert_eq!(price_data.price_cumulative.conf, 3 * 2 + 2 * 3 + 3);
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
        assert_eq!(price_data.agg_.price_, -100);
        assert_eq!(price_data.agg_.conf_, 1);
        assert_eq!(price_data.agg_.status_, PC_STATUS_UNKNOWN);

        assert_eq!(price_data.price_cumulative.price, 3 * 42 + 81 * 3 - 100 * 3);
        assert_eq!(price_data.price_cumulative.conf, 3 * 2 + 2 * 3 + 3);
        assert_eq!(price_data.price_cumulative.num_down_slots, 0);
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
        assert_eq!(price_data.agg_.price_, 60);
        assert_eq!(price_data.agg_.conf_, 4);
        assert_eq!(price_data.agg_.status_, PC_STATUS_TRADING);

        assert_eq!(
            price_data.price_cumulative.price,
            3 * 42 + 81 * 3 - 100 * 3 + 42 * 60
        );
        assert_eq!(price_data.price_cumulative.conf, 3 * 2 + 2 * 3 + 3 + 42 * 4);
        assert_eq!(price_data.price_cumulative.num_down_slots, 17);
    }

    Ok(())
}

#[test]
fn test_upd_works_with_unordered_publisher_set() -> Result<(), Box<dyn std::error::Error>> {
    let mut instruction_data = [0u8; size_of::<UpdPriceArgs>()];

    let program_id = Pubkey::new_unique();

    let mut price_setup = AccountSetup::new::<PriceAccount>(&program_id);
    let mut price_account = price_setup.as_account_info();
    price_account.is_signer = false;
    PriceAccount::initialize(&price_account, PC_VERSION).unwrap();

    let mut publishers_setup: Vec<_> = (0..20).map(|_| AccountSetup::new_funding()).collect();
    let mut publishers: Vec<_> = publishers_setup
        .iter_mut()
        .map(|s| s.as_account_info())
        .collect();

    publishers.sort_by_key(|x| x.key);

    {
        let mut price_data = load_checked::<PriceAccount>(&price_account, PC_VERSION).unwrap();
        price_data.num_ = 20;
        // Store the publishers in reverse order
        publishers
            .iter()
            .rev()
            .enumerate()
            .for_each(|(i, account)| {
                price_data.comp_[i].pub_ = *account.key;
            });
    }

    let mut clock_setup = AccountSetup::new_clock();
    let mut clock_account = clock_setup.as_account_info();
    clock_account.is_signer = false;
    clock_account.is_writable = false;

    update_clock_slot(&mut clock_account, 1);

    for (i, publisher) in publishers.iter().enumerate() {
        populate_instruction(&mut instruction_data, (i + 100) as i64, 10, 1);
        process_instruction(
            &program_id,
            &[
                publisher.clone(),
                price_account.clone(),
                clock_account.clone(),
            ],
            &instruction_data,
        )?;
    }

    update_clock_slot(&mut clock_account, 2);

    // Trigger the aggregate calculation by sending another price
    // update
    populate_instruction(&mut instruction_data, 100, 10, 2);
    process_instruction(
        &program_id,
        &[
            publishers[0].clone(),
            price_account.clone(),
            clock_account.clone(),
        ],
        &instruction_data,
    )?;


    let price_data = load_checked::<PriceAccount>(&price_account, PC_VERSION).unwrap();

    // The result will be the following only if all the
    // publishers prices are included in the aggregate.
    assert_eq!(price_data.valid_slot_, 1);
    assert_eq!(price_data.agg_.pub_slot_, 2);
    assert_eq!(price_data.agg_.price_, 109);
    assert_eq!(price_data.agg_.conf_, 8);
    assert_eq!(price_data.agg_.status_, PC_STATUS_TRADING);

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
