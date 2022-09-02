use quickcheck::Arbitrary;
use quickcheck_macros::quickcheck;
use solana_program::nonce::state::Data;
use std::collections::BTreeMap;
use std::mem::{
    align_of,
    size_of,
};

use crate::c_oracle_header::PriceAccount;
use crate::time_machine_types::{
    PriceAccountWrapper,
    SmaTracker,
    TimeMachineWrapper,
    THIRTY_MINUTES,
};

// #[test]
// fn unit_tests() {
//     let mut tracker = SmaTracker::<10> {
//         granularity  : 10,
//         threshold : TWENTY_SECONDS,
//         current_epoch_denominator : 0,
//         current_epoch_is_valid : false,
//         current_epoch_numerator : 0,
//         running_valid_entry_counter: [0u64; 10],
//         running_sum_of_price_averages : [0i128; 10],
//     };

//     for i in 0u32..6000 {
//         tracker.add_datapoint((i64::from(i), i64::from(i+1)), 1 , 20, (0,0)).unwrap();
//     }

//     println!("{:?}", tracker);

//     tracker.add_datapoint((6000, 6010), 21, 30, (0,0)).unwrap();
//     println!("{:?}", tracker);
//     tracker.add_datapoint((6010, 6080), 100, 30, (0,0)).unwrap();
//     println!("{:?}", tracker);

//     // let mut tracker2 = SmaTracker::<48> {
//     //     granularity  : THIRTY_MINUTES,
//     //     threshold : 20,
//     //     current_epoch_denominator : 0,
//     //     current_epoch_is_valid : false,
//     //     current_epoch_numerator : 0,
//     //     running_valid_entry_counter: [0u64; 48],
//     //     running_sum_of_price_averages : [0i128; 48],
//     // };

//     // for i in 0u32..6000 {
//     //     tracker2.add_datapoint((i64::from(i* 20), i64::from((i+1) * 20)), (u64::from(i* 21),
// u64::from((i+1) * 21)), (20, 20), (0,0)).unwrap();     // }
//     // println!("{:?}", tracker2);
//     // for i in 6000u32..6180 {
//     //     tracker2.add_datapoint((i64::from(i* 20), i64::from((i+1) * 20)), (u64::from(i) +
// 126000, u64::from(i+1) + 126000), (20, 20), (0,0)).unwrap();     // }


//     // println!("{:?}", tracker2);
//     // println!("{:?}", tracker2.running_sum_of_price_averages[19]);
//     // println!("{:?}", tracker2.running_valid_entry_counter[19]);


//     // let tracker : & mut SmaTracker<48> =  bytemuck::from_bytes_mut(&mut buffer.as_ptr());
//     // unsafe{
//     //     println!("{:?}", buffer.as_ptr() as usize);
//     //     println!("{:?}", align_of::<SmaTracker<48>>());
//     //     println!("{:?}", align_of::<PriceAccountWrapper>());
//     //     println!("{:?}", align_of::<PriceAccount>());

//     //     }
//     // tracker.initialize(THIRTY_MINUTES, TWENTY_SECONDS);

// }

#[derive(Clone, Debug, Copy)]
struct DataEvent {
    timegap:  i64,
    slot_gap: u64,
    price:    i64,
}

#[derive(Debug)]
pub struct DataPoint {
    last_two_times: (i64, i64),
    slot_gap:       u64,
    price:          i64,
}

impl Arbitrary for DataEvent {
    fn arbitrary(g: &mut quickcheck::Gen) -> Self {
        DataEvent {
            timegap:  i64::from(u8::arbitrary(g)),
            slot_gap: u64::from(u8::arbitrary(g)) + 1,
            price:    i64::arbitrary(g),
        }
    }
}

#[quickcheck]
fn test_add_and_delete(input: Vec<DataEvent>) -> bool {
    // No gaps, no skipped epochs
    let mut tracker1 = SmaTracker::<10> {
        granularity:                   0,
        threshold:                     0,
        current_epoch_denominator:     0,
        current_epoch_is_valid:        false,
        current_epoch_numerator:       0,
        running_valid_epoch_counter:   [0u64; 10],
        running_sum_of_price_averages: [0i128; 10],
    };

    tracker1.initialize(i64::from(u8::MAX), u64::from(u8::MAX));

    // Likely skipped, no gaps
    let mut tracker2 = SmaTracker::<10> {
        granularity:                   0,
        threshold:                     0,
        current_epoch_denominator:     0,
        current_epoch_is_valid:        false,
        current_epoch_numerator:       0,
        running_valid_epoch_counter:   [0u64; 10],
        running_sum_of_price_averages: [0i128; 10],
    };

    tracker2.initialize(i64::from(u8::MAX / 5), u64::from(u8::MAX / 5));

    // Gaps
    let mut tracker3 = SmaTracker::<10> {
        granularity:                   0,
        threshold:                     0,
        current_epoch_denominator:     0,
        current_epoch_is_valid:        false,
        current_epoch_numerator:       0,
        running_valid_epoch_counter:   [0u64; 10],
        running_sum_of_price_averages: [0i128; 10],
    };

    tracker3.initialize(i64::from(u8::MAX), u64::from(u8::MAX / 5));

    let mut tracker4 = SmaTracker::<10> {
        granularity:                   0,
        threshold:                     0,
        current_epoch_denominator:     0,
        current_epoch_is_valid:        false,
        current_epoch_numerator:       0,
        running_valid_epoch_counter:   [0u64; 10],
        running_sum_of_price_averages: [0i128; 10],
    };

    tracker4.initialize(i64::from(u8::MAX / 5), 250);

    let mut tracker5 = SmaTracker::<10> {
        granularity:                   0,
        threshold:                     0,
        current_epoch_denominator:     0,
        current_epoch_is_valid:        false,
        current_epoch_numerator:       0,
        running_valid_epoch_counter:   [0u64; 10],
        running_sum_of_price_averages: [0i128; 10],
    };

    tracker5.initialize(1, u64::from(u8::MAX / 5));

    let mut data = Vec::<DataPoint>::new();

    let mut initial_time = 0i64;
    for data_event in input.clone() {
        tracker1
            .add_datapoint(
                (initial_time, initial_time + data_event.timegap),
                data_event.slot_gap,
                data_event.price,
                (0, 0),
            )
            .unwrap();
        tracker2
            .add_datapoint(
                (initial_time, initial_time + data_event.timegap),
                data_event.slot_gap,
                data_event.price,
                (0, 0),
            )
            .unwrap();
        tracker3
            .add_datapoint(
                (initial_time, initial_time + data_event.timegap),
                data_event.slot_gap,
                data_event.price,
                (0, 0),
            )
            .unwrap();

        tracker4
            .add_datapoint(
                (initial_time, initial_time + data_event.timegap),
                data_event.slot_gap,
                data_event.price,
                (0, 0),
            )
            .unwrap();
        tracker5
            .add_datapoint(
                (initial_time, initial_time + data_event.timegap),
                data_event.slot_gap,
                data_event.price,
                (0, 0),
            )
            .unwrap();
        data.push(DataPoint {
            last_two_times: (initial_time, initial_time + data_event.timegap),
            slot_gap:       data_event.slot_gap,
            price:          data_event.price,
        });
        initial_time += data_event.timegap;

        check_epoch_fields(&tracker1, &data, initial_time);
        check_epoch_fields(&tracker2, &data, initial_time);
        check_epoch_fields(&tracker3, &data, initial_time);
        check_epoch_fields(&tracker4, &data, initial_time);
        check_epoch_fields(&tracker5, &data, initial_time);

        check_all_fields(&tracker1, &data, initial_time);
        check_all_fields(&tracker2, &data, initial_time);
        check_all_fields(&tracker3, &data, initial_time);
        check_all_fields(&tracker4, &data, initial_time);
        check_all_fields(&tracker5, &data, initial_time);
    }


    // println!("{:?}", tracker1);


    return true;
}

pub fn check_epoch_fields(tracker: &SmaTracker<10>, data: &Vec<DataPoint>, time: i64) {
    let last_epoch_1 = tracker.time_to_epoch(time).unwrap();

    let result = compute_epoch_fields(tracker, data, last_epoch_1);
    assert_eq!(tracker.current_epoch_denominator, result.0);
    assert_eq!(tracker.current_epoch_numerator, result.1);
    assert_eq!(tracker.current_epoch_is_valid, result.2);
}
pub fn check_all_fields(tracker: &SmaTracker<10>, data: &Vec<DataPoint>, time: i64) {
    let last_epoch_1 = tracker.time_to_epoch(time).unwrap();
    let mut values = vec![];

    for i in 0..last_epoch_1 {
        values.push(compute_epoch_fields(tracker, data, i));
    }

    let iter = values.iter().scan(0, |state, &y| {
        *state = *state + y.1.clone() / i128::from(y.0.clone());
        Some(*state)
    });

    let mut i = (last_epoch_1 + 9) % 10;

    let collected = iter.collect::<Vec<i128>>();
    for x in collected.iter().rev().take(9) {
        assert_eq!(tracker.running_sum_of_price_averages[i], *x);
        i = (i + 9) % 10;
    }
}

pub fn compute_epoch_fields(
    tracker: &SmaTracker<10>,
    data: &Vec<DataPoint>,
    epoch_number: usize,
) -> (u64, i128, bool) {
    let left_bound = epoch_number
        .checked_mul(tracker.granularity.try_into().unwrap())
        .unwrap();

    let right_bound = (epoch_number + 1)
        .checked_mul(tracker.granularity.try_into().unwrap())
        .unwrap();


    let result = data.iter().fold((0, 0, true), |x: (u64, i128, bool), y| {
        if y.last_two_times.0 == y.last_two_times.1 {}
        if !((left_bound > y.last_two_times.1.try_into().unwrap())
            || (right_bound <= y.last_two_times.0.try_into().unwrap()))
        {
            let is_valid = y.slot_gap <= tracker.threshold;
            return (
                x.0 + y.slot_gap,
                x.1 + i128::from(y.slot_gap) * i128::from(y.price),
                x.2 && is_valid,
            );
        }
        return x;
    });
    return result;
}
