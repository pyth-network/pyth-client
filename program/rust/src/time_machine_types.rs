#![allow(unused_imports)]
#![allow(dead_code)]
use crate::c_oracle_header::{
    PriceAccount,
    PythAccount,
    EXTRA_PUBLISHER_SPACE,
    PC_ACCTYPE_PRICE,
    PC_MAX_SEND_LATENCY,
    PC_PRICE_T_COMP_OFFSET,
};
use crate::error::OracleError;
use crate::utils::try_convert;
use bytemuck::{
    Pod,
    Zeroable,
};

pub const THIRTY_MINUTES: i64 = 30 * 60;
pub const NUM_BUCKETS_THIRTY_MIN: usize = 48;
#[derive(Copy, Clone)]
#[repr(C)]
pub struct PriceAccountWrapper {
    // An instance of price account
    pub price_data:            PriceAccount,
    /// Space for more publishers
    pub extra_publisher_space: [u8; EXTRA_PUBLISHER_SPACE as usize],
    // TimeMachine
    pub time_machine:          SmaTracker<NUM_BUCKETS_THIRTY_MIN>,
}


#[derive(Debug)]
pub struct DataPoint {
    pub last_two_timestamps: (i64, i64),
    pub slot_gap:            u64,
    pub price:               i64,
}

impl PriceAccountWrapper {
    pub fn initialize_time_machine(&mut self) -> Result<(), OracleError> {
        #[cfg(test)]
        self.time_machine
            .initialize(THIRTY_MINUTES, PC_MAX_SEND_LATENCY.into());
        Ok(())
    }

    pub fn add_price_to_time_machine(&mut self) -> Result<(), OracleError> {
        #[cfg(test)]
        self.time_machine.add_datapoint( &DataPoint{
            last_two_timestamps : (self.price_data.prev_timestamp_, self.price_data.timestamp_),
            slot_gap : (self.price_data.last_slot_ - self.price_data.prev_slot_),
            price: self.price_data.prev_price_ /2  + self.price_data.agg_.price_ / 2 + (self.price_data.prev_price_ % 2) * (self.price_data.agg_.price_ % 2), // Hack to avoid overflow?
        }
        )?;
        Ok(())
    }
}

#[derive(Debug, Copy, Clone)]
#[repr(C)]
pub struct SmaTracker<const NUM_BUCKETS: usize> {
    /// The maximum gap in slots for an epoch's sma to be valid, we set it to PC_MAX_SEND_LATENCY
    pub threshold:                     u64,
    /// The legth of one sma epoch.
    pub granularity:                   i64,
    /// Numerator for the current epoch (ex : slot_gap1 * price_1 + slot_gap2 * price_2 + slot_gap3
    /// * price_3)
    /// It can never overflow because :
    /// slot_gap1 * price_1 + slot_gap2 * price_2 + slot_gap3 * price_3 <= (slot_gap1 + slot_gap2 +
    /// slot_gap3) * i64::MAX <= clock_slot * i128::MAX
    pub current_epoch_numerator:       i128,
    /// Denominator for the current epoch (ex : slot_gap1 + slot_gap2 + slot_gap3)
    /// It can never overflow because : (slot_gap1 + slot_gap2 + slot_gap3) <= clock_slot
    pub current_epoch_denominator:     u64,
    /// Whether the current epoch is valid (i.e. max slot_gap <= threshold) for all the epochs
    /// updates
    pub current_epoch_is_valid:        bool,
    /// Stores the running sum of individual epoch averages, we can support u64::MAX epochs. And
    /// clock.unix_timestamp <= u64::MAX so the clock will break first.
    pub running_sum_of_price_averages: [i128; NUM_BUCKETS],
    /// Stores the number of valid epochs since inception, we can support u64::MAX epochs. And
    /// clock.unix_timestamp <= u64::MAX so the clock will break first.
    pub running_valid_epoch_counter:   [u64; NUM_BUCKETS],
}

impl<const NUM_BUCKETS: usize> SmaTracker<NUM_BUCKETS> {
    pub fn time_to_epoch(&self, time: i64) -> Result<usize, OracleError> {
        try_convert::<i64, usize>(time / self.granularity) // Can never fail because usize is u64
                                                           // and time is positive
    }

    pub fn initialize(&mut self, granularity: i64, threshold: u64) {
        self.threshold = threshold;
        self.granularity = granularity;
        self.current_epoch_is_valid = true;
    }

    pub fn add_datapoint(&mut self, datapoint: &DataPoint) -> Result<(), OracleError> {
        let epoch_0 = self.time_to_epoch(datapoint.last_two_timestamps.0)?;
        let epoch_1 = self.time_to_epoch(datapoint.last_two_timestamps.1)?;

        let datapoint_numerator = i128::from(datapoint.slot_gap) * i128::from(datapoint.price);
        let datapoint_denominator = datapoint.slot_gap;

        self.current_epoch_denominator += datapoint_denominator;
        self.current_epoch_numerator += datapoint_numerator;

        self.current_epoch_is_valid =
            self.current_epoch_is_valid && datapoint_denominator <= self.threshold;

        // If epoch changed
        if epoch_0 < epoch_1 {
            let index = epoch_0.rem_euclid(NUM_BUCKETS);

            // This addition can only overflow if NUM_BUCKETS ~ u64::MAX / 2
            let prev_index = (epoch_0 + NUM_BUCKETS - 1).rem_euclid(NUM_BUCKETS);

            self.running_sum_of_price_averages[index] = self.running_sum_of_price_averages
                [prev_index]
                + self.current_epoch_numerator / i128::from(self.current_epoch_denominator);
            // The fraction here is smaller than i64::MAX , so we can support u64::MAX updates

            if self.current_epoch_is_valid {
                self.running_valid_epoch_counter[index] =
                    self.running_valid_epoch_counter[prev_index] + 1; // Likewise can support
                                                                      // u64::MAX
                                                                      // epochs
            } else {
                self.running_valid_epoch_counter[index] =
                    self.running_valid_epoch_counter[prev_index]
            };

            self.current_epoch_denominator = datapoint_denominator;
            self.current_epoch_numerator = datapoint_numerator;
            self.current_epoch_is_valid = datapoint_denominator <= self.threshold;
        }

        // If at least one epoch has been skipped
        if epoch_0 + 1 < epoch_1 {
            let one_point_average = datapoint_numerator / i128::from(datapoint_denominator);
            let mut i = 1;
            let mut current_bucket = (epoch_0 + 1) % NUM_BUCKETS;
            let bucket_0 = epoch_0 % NUM_BUCKETS;
            let bucket_1 = epoch_1 % NUM_BUCKETS;

            // Number of times we have wrapped around the buffer
            let number_of_full_wraparound = (epoch_1 - 1 - epoch_0) / NUM_BUCKETS;

            // These buckets are "inside" (bucket_0, bucket_1)
            while current_bucket != bucket_1 {
                self.running_sum_of_price_averages[current_bucket] = self
                    .running_sum_of_price_averages[bucket_0]
                    + ((i + number_of_full_wraparound * NUM_BUCKETS) as i128) * one_point_average;

                if self.current_epoch_is_valid {
                    self.running_valid_epoch_counter[current_bucket] = self
                        .running_valid_epoch_counter[bucket_0]
                        + ((i + number_of_full_wraparound * NUM_BUCKETS) as u64);
                } else {
                    self.running_valid_epoch_counter[current_bucket] =
                        self.running_valid_epoch_counter[bucket_0];
                }

                i += 1;
                current_bucket = (current_bucket + 1) % NUM_BUCKETS;
            }

            if number_of_full_wraparound > 0 {
                // These buckets are "outside" (bucket_0, bucket_1)
                while i != NUM_BUCKETS + 1 {
                    self.running_sum_of_price_averages[current_bucket] = self
                        .running_sum_of_price_averages[bucket_0]
                        + ((i + (number_of_full_wraparound - 1) * NUM_BUCKETS) as i128)
                            * one_point_average;

                    if self.current_epoch_is_valid {
                        self.running_valid_epoch_counter[current_bucket] = self
                            .running_valid_epoch_counter[bucket_0]
                            + ((i + (number_of_full_wraparound - 1) * NUM_BUCKETS) as u64);
                    } else {
                        self.running_valid_epoch_counter[current_bucket] =
                            self.running_valid_epoch_counter[bucket_0];
                    }
                    i += 1;
                    current_bucket = (current_bucket + 1) % NUM_BUCKETS;
                }
            }
        }

        Ok(())
    }
}

#[cfg(target_endian = "little")]
unsafe impl Zeroable for PriceAccountWrapper {
}

#[cfg(target_endian = "little")]
unsafe impl Pod for PriceAccountWrapper {
}

impl PythAccount for PriceAccountWrapper {
    const ACCOUNT_TYPE: u32 = PC_ACCTYPE_PRICE;
    const INITIAL_SIZE: u32 = PC_PRICE_T_COMP_OFFSET as u32;
}

#[cfg(test)]
pub mod tests {
    use crate::c_oracle_header::{
        PRICE_ACCOUNT_SIZE,
        TIME_MACHINE_STRUCT_SIZE,
    };
    use crate::time_machine_types::{
        PriceAccountWrapper,
        SmaTracker,
        NUM_BUCKETS_THIRTY_MIN,
    };
    use std::mem::size_of;
    #[test]
    ///test that the size defined in C matches that
    ///defined in Rust
    fn c_time_machine_size_is_correct() {
        assert_eq!(
            TIME_MACHINE_STRUCT_SIZE as usize,
            size_of::<SmaTracker<NUM_BUCKETS_THIRTY_MIN>>()
        );
    }
    #[test]
    ///test that priceAccountWrapper has a correct size
    fn c_price_account_size_is_correct() {
        assert_eq!(
        size_of::<PriceAccountWrapper>(),
        PRICE_ACCOUNT_SIZE as usize,
        "expected PRICE_ACCOUNT_SIZE ({}) in oracle.h to the same as the size of PriceAccountWrapper ({})",
        PRICE_ACCOUNT_SIZE,
        size_of::<PriceAccountWrapper>()
    );
    }
}
