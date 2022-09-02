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
    DataPoint,
    PriceAccountExtended,
    SmaTracker,
    NUM_BUCKETS_THIRTY_MIN,
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
    let mut tracker1 = SmaTracker::<NUM_BUCKETS_THIRTY_MIN>::zero();
    tracker1.initialize(i64::from(u8::MAX), u64::from(u8::MAX));

    // Skipped and gaps
    let mut tracker2 = SmaTracker::<NUM_BUCKETS_THIRTY_MIN>::zero();
    tracker2.initialize(i64::from(u8::MAX / 5), u64::from(u8::MAX / 5));

    // Gaps, no skips
    let mut tracker3 = SmaTracker::<NUM_BUCKETS_THIRTY_MIN>::zero();
    tracker3.initialize(i64::from(u8::MAX), u64::from(u8::MAX / 5));

    // No skips, gaps
    let mut tracker4 = SmaTracker::<NUM_BUCKETS_THIRTY_MIN>::zero();
    tracker4.initialize(i64::from(u8::MAX), u64::from(u8::MAX / 5) * 4);

    // Each epoch is 1 second
    let mut tracker5 = SmaTracker::<NUM_BUCKETS_THIRTY_MIN>::zero();
    tracker5.initialize(1, u64::from(u8::MAX / 5));

    let mut data = Vec::<DataPoint>::new();

    let mut current_time = 0i64;
    for data_event in input.clone() {
        let datapoint = DataPoint {
            last_two_timestamps: (current_time, current_time + data_event.timegap),
            slot_gap:            data_event.slot_gap,
            price:               data_event.price,
        };

        tracker1.add_datapoint(&datapoint).unwrap();
        tracker2.add_datapoint(&datapoint).unwrap();
        tracker3.add_datapoint(&datapoint).unwrap();

        tracker4.add_datapoint(&datapoint).unwrap();
        tracker5.add_datapoint(&datapoint).unwrap();
        data.push(datapoint);
        current_time += data_event.timegap;

        tracker1.check_current_epoch_fields(&data, current_time);
        tracker2.check_current_epoch_fields(&data, current_time);
        tracker3.check_current_epoch_fields(&data, current_time);
        tracker4.check_current_epoch_fields(&data, current_time);
        tracker5.check_current_epoch_fields(&data, current_time);
        tracker1.check_array_fields(&data, current_time);
        tracker2.check_array_fields(&data, current_time);
        tracker3.check_array_fields(&data, current_time);
        tracker4.check_array_fields(&data, current_time);
        tracker5.check_array_fields(&data, current_time);
    }

    return true;
}


impl<const NUM_ENTRIES: usize> SmaTracker<NUM_ENTRIES> {
    pub fn zero() -> Self {
        return SmaTracker::<NUM_ENTRIES> {
            granularity:                   0,
            threshold:                     0,
            current_epoch_denominator:     0,
            current_epoch_is_valid:        false,
            current_epoch_numerator:       0,
            running_valid_epoch_counter:   [0u64; NUM_ENTRIES],
            running_sum_of_price_averages: [0i128; NUM_ENTRIES],
        };
    }

    pub fn check_current_epoch_fields(&self, data: &Vec<DataPoint>, time: i64) {
        let curent_epoch = self.time_to_epoch(time).unwrap();

        let result = self.compute_epoch_expected_values(data, curent_epoch);
        assert_eq!(self.current_epoch_denominator, result.0);
        assert_eq!(self.current_epoch_numerator, result.1);
        assert_eq!(self.current_epoch_is_valid, result.2);
    }

    pub fn check_array_fields(&self, data: &Vec<DataPoint>, time: i64) {
        let current_epoch = self.time_to_epoch(time).unwrap();
        let mut values = vec![];

        // Compute all epoch averages
        for i in 0..current_epoch {
            values.push(self.compute_epoch_expected_values(data, i));
        }

        // Get running sums
        let running_sum_price_iter = values.iter().scan((0, 0), |res, &y| {
            res.0 = res.0 + y.1.clone() / i128::from(y.0.clone());
            res.1 = res.1 + u64::from(y.2);
            Some(*res)
        });

        // Compare to running_sum_of_price_averages
        let mut i = (current_epoch + NUM_ENTRIES - 1).rem_euclid(NUM_ENTRIES);
        for x in running_sum_price_iter
            .collect::<Vec<(i128, u64)>>()
            .iter()
            .rev()
            .take(NUM_ENTRIES)
        {
            assert_eq!(self.running_sum_of_price_averages[i], x.0);
            assert_eq!(self.running_valid_epoch_counter[i], x.1);
            i = (i + NUM_ENTRIES - 1).rem_euclid(NUM_ENTRIES);
        }
    }

    pub fn compute_epoch_expected_values(
        &self,
        data: &Vec<DataPoint>,
        epoch_number: usize,
    ) -> (u64, i128, bool) {
        let left_bound = self
            .granularity
            .checked_mul(epoch_number.try_into().unwrap())
            .unwrap();

        let right_bound = self
            .granularity
            .checked_mul((epoch_number + 1).try_into().unwrap())
            .unwrap();


        let result = data.iter().fold((0, 0, true), |x: (u64, i128, bool), y| {
            if !((left_bound > y.last_two_timestamps.1) || (right_bound <= y.last_two_timestamps.0))
            {
                let is_valid = y.slot_gap <= self.threshold;
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
}
