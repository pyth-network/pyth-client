extern crate test_generator;
use bytemuck::Zeroable;
use std::fs::File;
use crate::accounts::{PriceAccount};
use csv::ReaderBuilder;
use serde::{Deserialize, Serialize};
use crate::processor::{c_upd_aggregate, c_upd_twap};

use test_generator::test_resources;


#[test_resources("program/rust/test_data/twap/*.csv")]
fn test_quote_set(input_path_raw: &str) {
    // For some reason these tests have a different working directory than the macro.
    let input_path = input_path_raw.replace("program/rust/", "");
    let result_path = input_path.replace(".csv", ".result");

    let input_file = File::open(input_path).expect("Test file not found");
    let mut input_reader = ReaderBuilder::new().has_headers(true).from_reader(input_file);
    let inputs: Vec<InputRecord> = input_reader.deserialize::<InputRecord>().map(|record| record.expect("Could not parse CSV record")).collect();

    let result_file = File::open(result_path).expect("Test file not found");
    let mut result_reader = ReaderBuilder::new().has_headers(true).from_reader(result_file);
    let expected_outputs: Vec<OutputRecord> = result_reader.deserialize::<OutputRecord>().map(|record| record.expect("Could not parse CSV record")).collect();

    assert_eq!(inputs.len(), expected_outputs.len(), "Mismatched CSV file lengths");

    let current_slot = 1000;
    let current_timestamp = 1234;
    let mut price_account: PriceAccount = PriceAccount::zeroed();

    price_account.last_slot_ = current_slot;
    price_account.agg_.pub_slot_ = current_slot;
    price_account.num_ = 0;

    upd_aggregate(&mut price_account, current_slot + 1, current_timestamp);

    for (input, _expected_output) in inputs.iter().zip(expected_outputs.iter()) {
        price_account.exponent = input.expo;
        price_account.agg_.price_ = input.price;
        price_account.agg_.conf_ = input.conf;

        upd_twap(&mut price_account, input.nslots);
    }


    /*
    let quote_set: QuoteSet = serde_json::from_reader(&file).expect("Unable to parse JSON");

    let result_file = File::open(result_path).expect("Test file not found");
    let expected_result: QuoteSetResult = serde_json::from_reader(&result_file).expect("Unable to parse JSON");

    let current_slot = 1000; // arbitrary
    let current_timestamp = 1234; // also arbitrary
    let mut price_account: PriceAccount = PriceAccount::zeroed();

    price_account.last_slot_ = current_slot;
    price_account.agg_.pub_slot_ = current_slot;
    price_account.exponent = quote_set.exponent;
    price_account.num_ = quote_set.quotes.len() as u32;
    for quote_idx in 0..quote_set.quotes.len() {
        let mut current_component = &mut price_account.comp_[quote_idx];
        let quote = &quote_set.quotes[quote_idx];
        current_component.latest_.status_ = quote.status;
        current_component.latest_.price_ = quote.price;
        current_component.latest_.conf_ = quote.conf;
        let slot_diff = quote.slot_diff.unwrap_or(0);
        assert!(slot_diff > current_slot as i64 * -1);
        current_component.latest_.pub_slot_ = ((current_slot as i64) + slot_diff) as u64;
    }

    unsafe {
        c_upd_aggregate(
            (&mut price_account as *mut PriceAccount) as *mut u8,
            current_slot + 1,
            current_timestamp,
        );
    }

    // For some idiotic reason the status in the input is a number and the output is a string.
    let result_status: String = match price_account.agg_.status_ {
        0 => "unknown",
        1 => "trading",
        2 => "halted",
        3 => "auction",
        4 => "ignored",
        _ => "invalid status"
    }.into();

    let actual_result = QuoteSetResult {
        exponent: price_account.exponent,
        price: price_account.agg_.price_,
        conf: price_account.agg_.conf_,
        status: result_status
    };

    assert_eq!(expected_result, actual_result);
     */
}

// TODO: put these functions somewhere more accessible
pub fn upd_aggregate(price_account: &mut PriceAccount, clock_slot: u64, clock_timestamp: i64) -> bool {
    unsafe {
        return c_upd_aggregate(
            (price_account as *mut PriceAccount) as *mut u8,
            clock_slot,
            clock_timestamp,
        );
    }
}

pub fn upd_twap(price_account: &mut PriceAccount, nslots: i64) {
    unsafe {
        c_upd_twap((price_account as *mut PriceAccount) as *mut u8, nslots)
    }
}


#[derive(Serialize, Deserialize, Debug)]
struct InputRecord {
    price: i64,
    conf: u64,
    expo: i32,
    nslots: i64,
}

#[derive(Serialize, Deserialize, Debug)]
struct OutputRecord {
    price: i64,
    conf: u64,
    expo: i32,
    nslots: i64,
    twap: i64,
    twac: i64,
}

