use {
    crate::time_machine_types::{
        DataPoint,
        SmaTracker,
        NUM_BUCKETS_THIRTY_MIN,
    },
    quickcheck::Arbitrary,
    quickcheck_macros::quickcheck,
};

#[derive(Clone, Debug, Copy)]
struct DataEvent {
    time_gap: i64,
    slot_gap: u64,
    price:    i64,
}

impl Arbitrary for DataEvent {
    fn arbitrary(g: &mut quickcheck::Gen) -> Self {
        DataEvent {
            time_gap: i64::from(u8::arbitrary(g)),
            slot_gap: u64::from(u8::arbitrary(g)) + 1, /* Slot gap is always > 1, because there
                                                        * has been a succesful aggregation */
            price:    i64::arbitrary(g),
        }
    }
}

/// This is a generative test for the sma struct. quickcheck will generate a series of
/// vectors of DataEvents of different length. The generation is based on the arbitrary trait
/// above.
/// For each DataEvent :
/// - time_gap is a random number between 0 and u8::MAX (255)
/// - slot_gap is a random number between 1 and u8::MAX + 1 (256)
/// - price is a random i64
#[quickcheck]
fn test_sma(input: Vec<DataEvent>) -> bool {
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
    for data_event in input {
        let datapoint = DataPoint {
            previous_timestamp: current_time,
            current_timestamp:  current_time + data_event.time_gap,
            slot_gap:           data_event.slot_gap,
            price:              data_event.price,
        };

        tracker1.add_datapoint(&datapoint).unwrap();
        tracker2.add_datapoint(&datapoint).unwrap();
        tracker3.add_datapoint(&datapoint).unwrap();
        tracker4.add_datapoint(&datapoint).unwrap();
        tracker5.add_datapoint(&datapoint).unwrap();
        data.push(datapoint);
        current_time += data_event.time_gap;

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

    true
}


impl<const NUM_ENTRIES: usize> SmaTracker<NUM_ENTRIES> {
    pub fn zero() -> Self {
        SmaTracker::<NUM_ENTRIES> {
            granularity:                   0,
            threshold:                     0,
            current_epoch_denominator:     0,
            current_epoch_is_valid:        false,
            current_epoch_numerator:       0,
            running_valid_epoch_counter:   [0u64; NUM_ENTRIES],
            running_sum_of_price_averages: [0i128; NUM_ENTRIES],
        }
    }

    pub fn check_current_epoch_fields(&self, data: &[DataPoint], time: i64) {
        let curent_epoch = self.time_to_epoch(time).unwrap();

        let result = self.compute_epoch_expected_values(data, curent_epoch);
        assert_eq!(self.current_epoch_denominator, result.0);
        assert_eq!(self.current_epoch_numerator, result.1);
        assert_eq!(self.current_epoch_is_valid, result.2);
    }

    pub fn check_array_fields(&self, data: &[DataPoint], time: i64) {
        let current_epoch = self.time_to_epoch(time).unwrap();
        let mut values = vec![];

        // Compute all epoch averages
        for i in 0..current_epoch {
            values.push(self.compute_epoch_expected_values(data, i));
        }

        // Get running sums
        let running_sum_price_iter = values.iter().scan((0, 0), |res, &y| {
            res.0 += y.1 / i128::from(y.0);
            res.1 += u64::from(y.2);
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
        data: &[DataPoint],
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


        let mut result = data.iter().fold((0, 0, true), |x: (u64, i128, bool), y| {
            // Check interval intersection
            if !((left_bound > y.current_timestamp) || (right_bound <= y.previous_timestamp)) {
                let is_valid = y.slot_gap <= self.threshold;
                return (
                    x.0 + y.slot_gap,
                    x.1 + i128::from(y.slot_gap) * i128::from(y.price),
                    x.2 && is_valid,
                );
            }
            x
        });

        if epoch_number == 0 {
            result.2 = false;
        }

        result
    }
}
