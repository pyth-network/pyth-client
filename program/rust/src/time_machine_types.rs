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
pub struct PriceAccountExtended {
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

impl PriceAccountExtended {
    pub fn initialize(&mut self) -> Result<(), OracleError> {
        self.time_machine
            .initialize(THIRTY_MINUTES, PC_MAX_SEND_LATENCY.into());
        Ok(())
    }

    pub fn add_datapoint(&mut self) -> Result<(), OracleError> {
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
    pub threshold:                     u64,
    pub granularity:                   i64,
    pub current_epoch_numerator:       i128,
    pub current_epoch_denominator:     u64,
    pub current_epoch_is_valid:        bool,
    pub running_sum_of_price_averages: [i128; NUM_BUCKETS],
    pub running_valid_epoch_counter:   [u64; NUM_BUCKETS],
}

impl<const NUM_ENTRIES: usize> SmaTracker<NUM_ENTRIES> {
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

        let datapoint_numerator = i128::from(datapoint.slot_gap) * i128::from(datapoint.price); //Can't overflow because u64::MAX * i64::MAX = i28::MAX
        let datapoint_denominator = datapoint.slot_gap;

        self.current_epoch_denominator += datapoint_denominator; // Can't overflow, it's always smaller than the current solana slot
        self.current_epoch_numerator += datapoint_numerator; // Can't overflow, it's always smaller than u64::MAX * i64::MAX = i28::MAX,
                                                             // self.current_epoch_numerator = slot_gap1 * price1 + slot_gap2 * price2 + slot_gap3 *
                                                             // price3 <= (slot_gap1 + slot_gap2 + slot_gap3) * i64::MAX <= u64::MAX * i64::MAX
                                                             // =i128::MAX
        self.current_epoch_is_valid =
            self.current_epoch_is_valid && datapoint_denominator <= self.threshold;

        // This for loop is highly inefficient, but this is what the behavior should be
        // It updates all the epochs that got skipped
        // This will always yield an estimate of the average, even for invalid epochs

        if epoch_0 < epoch_1 {
            let index = epoch_0.rem_euclid(NUM_ENTRIES);
            let prev_index = (epoch_0 + NUM_ENTRIES - 1).rem_euclid(NUM_ENTRIES); // epoch <= time <= i64::MAX <= u64::MAX / 2, so this will only overflow if NUM_ENTRIES
                                                                                  // ~
                                                                                  // u64::MAX / 2

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

            let fraction = datapoint_numerator / i128::from(datapoint_denominator);
            if epoch_0 + 1 < epoch_1 {
                for i in 1..NUM_ENTRIES + 1 {
                    let current_index = (epoch_0 + i) % NUM_ENTRIES;
                    let number_of_time_skipped =
                        self.number_times_skipped(current_index, epoch_0, epoch_1);
                    if number_of_time_skipped > 0 {
                        self.running_sum_of_price_averages[current_index % NUM_ENTRIES] = self
                            .running_sum_of_price_averages[(epoch_0) % NUM_ENTRIES]
                            + ((i + (number_of_time_skipped - 1) * NUM_ENTRIES) as i128) * fraction;

                        if self.current_epoch_is_valid {
                            self.running_valid_epoch_counter[current_index % NUM_ENTRIES] = self
                                .running_valid_epoch_counter[(epoch_0) % NUM_ENTRIES]
                                + (i as u64)
                                + ((number_of_time_skipped - 1) as u64) * (NUM_ENTRIES as u64);
                        } else {
                            self.running_valid_epoch_counter[current_index % NUM_ENTRIES] = self
                                .running_valid_epoch_counter
                                [(epoch_0 + i + NUM_ENTRIES - 1) % NUM_ENTRIES];
                        }
                    }
                }
            }
        }

        Ok(())
    }

    pub fn number_times_skipped(&self, i: usize, epoch_0: usize, epoch_1: usize) -> usize {
        let is_between = ((epoch_0 % NUM_ENTRIES) < i && i < (epoch_1 % NUM_ENTRIES))
            || ((epoch_1 % NUM_ENTRIES) <= (epoch_0 % NUM_ENTRIES))
                && ((i < (epoch_1 % NUM_ENTRIES)) || (i > (epoch_0 % NUM_ENTRIES)));
        (epoch_1 - 1 - epoch_0) / NUM_ENTRIES + usize::from(is_between)
    }
}

#[cfg(target_endian = "little")]
unsafe impl Zeroable for PriceAccountExtended {
}

#[cfg(target_endian = "little")]
unsafe impl Pod for PriceAccountExtended {
}

#[cfg(target_endian = "little")]
unsafe impl Zeroable for SmaTracker<NUM_BUCKETS_THIRTY_MIN> {
}

#[cfg(target_endian = "little")]
unsafe impl Pod for SmaTracker<NUM_BUCKETS_THIRTY_MIN> {
}

impl PythAccount for PriceAccountExtended {
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
        PriceAccountExtended,
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
        size_of::<PriceAccountExtended>(),
        PRICE_ACCOUNT_SIZE as usize,
        "expected PRICE_ACCOUNT_SIZE ({}) in oracle.h to the same as the size of PriceAccountWrapper ({})",
        PRICE_ACCOUNT_SIZE,
        size_of::<PriceAccountExtended>()
    );
    }
}
