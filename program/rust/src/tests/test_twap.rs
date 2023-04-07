extern crate test_generator;
use {
    crate::{
        accounts::PriceAccount,
        processor::{
            c_upd_aggregate,
            c_upd_twap,
        },
    },
    bytemuck::Zeroable,
    csv::ReaderBuilder,
    serde::{
        Deserialize,
        Serialize,
    },
    std::fs::File,
    test_generator::test_resources,
};


#[test_resources("program/rust/test_data/twap/*.csv")]
fn test_quote_set(input_path_raw: &str) {
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
