extern crate test_generator;

use {
    super::test_utils::update_clock_slot,
    crate::{
        accounts::{
            PriceAccount,
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
        processor::{
            c_upd_aggregate,
            c_upd_twap,
            process_instruction,
        },
        tests::test_utils::AccountSetup,
    },
    bytemuck::Zeroable,
    csv::ReaderBuilder,
    serde::{
        Deserialize,
        Serialize,
    },
    solana_program::pubkey::Pubkey,
    std::{
        fs::File,
        mem::size_of,
    },
    test_generator::test_resources,
};

#[test]
fn test_ema_multiple_publishers_same_slot() -> Result<(), Box<dyn std::error::Error>> {
    let mut instruction_data = [0u8; size_of::<UpdPriceArgs>()];
    populate_instruction(&mut instruction_data, 10, 1, 1);

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
        assert_eq!(price_data.prev_twap_.val_, 0);
        assert_eq!(price_data.prev_twac_.val_, 0);
        assert_eq!(price_data.twap_.val_, 10);
        assert_eq!(price_data.twac_.val_, 1);
    }

    // add new test for multiple publishers and ensure that ema is updated correctly
    let mut funding_setup_two = AccountSetup::new_funding();
    let funding_account_two = funding_setup_two.as_account_info();

    {
        let mut price_data = load_checked::<PriceAccount>(&price_account, PC_VERSION).unwrap();
        price_data.num_ = 2;
        price_data.comp_[1].pub_ = *funding_account_two.key;
    }

    update_clock_slot(&mut clock_account, 2);

    populate_instruction(&mut instruction_data, 20, 1, 2);
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
        assert_eq!(price_data.prev_twap_.val_, 10);
        assert_eq!(price_data.prev_twac_.val_, 1);
        assert_eq!(price_data.twap_.val_, 15);
        assert_eq!(price_data.twac_.val_, 1);
    }

    populate_instruction(&mut instruction_data, 30, 1, 2);
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
        assert_eq!(price_data.prev_twap_.val_, 10);
        assert_eq!(price_data.prev_twac_.val_, 1);
        assert_eq!(price_data.twap_.val_, 12);
        assert_eq!(price_data.twac_.val_, 1);
    }

    // add test for multiple publishers where the first publisher causes the aggregate to be unknown and second publisher causes it to be trading
    {
        let mut price_data = load_checked::<PriceAccount>(&price_account, PC_VERSION).unwrap();
        price_data.min_pub_ = 2;
        price_data.num_ = 1;
    }

    update_clock_slot(&mut clock_account, 3);

    populate_instruction(&mut instruction_data, 20, 1, 3);
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
        assert_eq!(price_data.agg_.status_, PC_STATUS_UNKNOWN);
        assert_eq!(price_data.prev_twap_.val_, 10);
        assert_eq!(price_data.prev_twac_.val_, 1);
        assert_eq!(price_data.twap_.val_, 12);
        assert_eq!(price_data.twac_.val_, 1);
    }

    {
        let mut price_data = load_checked::<PriceAccount>(&price_account, PC_VERSION).unwrap();
        price_data.min_pub_ = 2;
        price_data.num_ = 2;
    }

    populate_instruction(&mut instruction_data, 40, 1, 3);
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
        assert_eq!(price_data.agg_.status_, PC_STATUS_TRADING);
        assert_eq!(price_data.prev_twap_.val_, 12);
        assert_eq!(price_data.prev_twac_.val_, 1);
        assert_eq!(price_data.twap_.val_, 13);
        assert_eq!(price_data.twac_.val_, 2);
    }
    Ok(())
}

#[test_resources("program/rust/test_data/ema/*.csv")]
fn test_ema(input_path_raw: &str) {
    let (inputs, expected_outputs) = read_test_data(input_path_raw);
    run_ema_test(&inputs, &expected_outputs);
}

// This test is identical to the one above, except that it negates all of the prices.
#[test_resources("program/rust/test_data/ema/*.csv")]
fn test_ema_negated(input_path_raw: &str) {
    let (inputs, expected_outputs) = read_test_data(input_path_raw);

    let modified_inputs: Vec<InputRecord> = inputs
        .iter()
        .map(|x| InputRecord {
            price:  -x.price,
            conf:   x.conf,
            expo:   x.expo,
            nslots: x.nslots,
        })
        .collect();

    let modified_outputs: Vec<OutputRecord> = expected_outputs
        .iter()
        .map(|x| OutputRecord {
            price:  -x.price,
            conf:   x.conf,
            expo:   x.expo,
            nslots: x.nslots,
            twap:   -x.twap,
            twac:   x.twac,
        })
        .collect();

    run_ema_test(&modified_inputs, &modified_outputs);
}

fn read_test_data(input_path_raw: &str) -> (Vec<InputRecord>, Vec<OutputRecord>) {
    // For some reason these tests have a different working directory than the macro.
    let input_path = input_path_raw.replace("program/rust/", "");
    let result_path = input_path.replace(".csv", ".result");

    let input_file = File::open(input_path).expect("Test file not found");
    let mut input_reader = ReaderBuilder::new()
        .has_headers(true)
        .from_reader(input_file);
    let inputs: Vec<InputRecord> = input_reader
        .deserialize::<InputRecord>()
        .map(|record| record.expect("Could not parse CSV record"))
        .collect();

    let result_file = File::open(result_path).expect("Test file not found");
    let mut result_reader = ReaderBuilder::new()
        .has_headers(true)
        .from_reader(result_file);
    let expected_outputs: Vec<OutputRecord> = result_reader
        .deserialize::<OutputRecord>()
        .map(|record| record.expect("Could not parse CSV record"))
        .collect();

    assert_eq!(
        inputs.len(),
        expected_outputs.len(),
        "Mismatched CSV file lengths"
    );

    (inputs, expected_outputs)
}

fn run_ema_test(inputs: &[InputRecord], expected_outputs: &[OutputRecord]) {
    let current_slot = 1000;
    let current_timestamp = 1234;
    let mut price_account: PriceAccount = PriceAccount::zeroed();

    price_account.last_slot_ = current_slot;
    price_account.agg_.pub_slot_ = current_slot;
    price_account.num_ = 0;

    upd_aggregate(&mut price_account, current_slot + 1, current_timestamp);

    for (input, expected_output) in inputs.iter().zip(expected_outputs.iter()) {
        price_account.exponent = input.expo;
        price_account.agg_.price_ = input.price;
        price_account.agg_.conf_ = input.conf;

        upd_twap(&mut price_account, input.nslots);

        assert_eq!(expected_output.twap, price_account.twap_.val_);
        assert_eq!(expected_output.twac, price_account.twac_.val_);
    }
}

// TODO: put these functions somewhere more accessible
pub fn upd_aggregate(
    price_account: &mut PriceAccount,
    clock_slot: u64,
    clock_timestamp: i64,
) -> bool {
    unsafe {
        c_upd_aggregate(
            (price_account as *mut PriceAccount) as *mut u8,
            clock_slot,
            clock_timestamp,
        )
    }
}

pub fn upd_twap(price_account: &mut PriceAccount, nslots: i64) {
    unsafe { c_upd_twap((price_account as *mut PriceAccount) as *mut u8, nslots) }
}

#[derive(Serialize, Deserialize, Debug)]
struct InputRecord {
    price:  i64,
    conf:   u64,
    expo:   i32,
    nslots: i64,
}

#[derive(Serialize, Deserialize, Debug)]
struct OutputRecord {
    price:  i64,
    conf:   u64,
    expo:   i32,
    nslots: i64,
    twap:   i64,
    twac:   i64,
}

fn populate_instruction(instruction_data: &mut [u8], price: i64, conf: u64, pub_slot: u64) {
    let mut cmd = load_mut::<UpdPriceArgs>(instruction_data).unwrap();
    cmd.header = OracleCommand::UpdPrice.into();
    cmd.status = PC_STATUS_TRADING;
    cmd.price = price;
    cmd.confidence = conf;
    cmd.publishing_slot = pub_slot;
    cmd.unused_ = 0;
}
