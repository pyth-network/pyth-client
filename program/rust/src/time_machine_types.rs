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

#[derive(Debug, Clone, Copy)]
#[repr(C)]
/// this wraps multiple SMA and tick trackers, and includes all the state
/// used by the time machine
pub struct TimeMachineWrapper {
    //Place holder with the size of the fields I am planning to add
    place_holder: [u8; 1864],
}

#[derive(Copy, Clone)]
#[repr(C)]
/// wraps everything stored in a price account
pub struct PriceAccountWrapper {
    //an instance of the c price_t type
    pub price_data:            PriceAccount,
    //space for more publishers
    pub extra_publisher_space: [u8; EXTRA_PUBLISHER_SPACE as usize],
    //TimeMachine
    pub time_machine:          SmaTracker<48>,
}
impl PriceAccountWrapper {
    pub fn initialize_time_machine(&mut self) -> Result<(), OracleError> {
        self.time_machine
            .initialize(THIRTY_MINUTES, PC_MAX_SEND_LATENCY.into());
        Ok(())
    }

    pub fn add_price_to_time_machine(&mut self) -> Result<(), OracleError> {
        self.time_machine.add_datapoint(
            (self.price_data.prev_timestamp_, self.price_data.timestamp_),
            self.price_data.last_slot_ - self.price_data.prev_slot_,
            (self.price_data.prev_price_ + self.price_data.agg_.price_) / 2,
            (0, 0),
        )?;
        Ok(())
    }
}

pub const THIRTY_MINUTES: i64 = 30 * 60;


#[derive(Debug, Copy, Clone)]
#[repr(C)]
/// Represents a Simple Moving Average Tracker Tracker that has NUM_ENTRIES entries
/// each tracking time weighted sums for granularity seconds periods.
///The prices are assumed to be provided under some fixed point representation, and the computation
/// guarantees accuracy up to the last decimal digit in the fixed point representation.
/// Assumes threshold < granularity
/// Both threshold and granularity are set on initialization
pub struct SmaTracker<const NUM_ENTRIES: usize> {
    pub threshold:                     u64, /* the maximum slot gap we are willing
                                             * to accept */
    pub granularity:                   i64, //the length of the time covered per entry
    pub current_epoch_numerator:       i128, /* accumulates
                                             * slot_delta *
                                             * (inflated_p1 +
                                             * inflated_p2) / 2
                                             * to compute
                                             * averages */
    pub current_epoch_denominator:     u64, /* accumulates the number of slots that
                                             * went into the
                                             * weights of the current entry, reset
                                             * to 0 before
                                             * moving to the next one */
    pub current_epoch_is_valid:        bool, /* the status of the current entry,
                                              *                                                        * INVALID if an update takes too
                                              *                                                        * long, PENDING if it is still being
                                              *                                                        * updated but the updates have been
                                              *                                                        * consistent so far. */
    pub running_sum_of_price_averages: [i128; NUM_ENTRIES], /* A running sum of the
                                                             * slot weighted average
                                                             * of each entry. */
    pub running_valid_entry_counter:   [u64; NUM_ENTRIES], /* Each entry
                                                            *                                                                        * increment the
                                                            *                                                                        * counter of the
                                                            *                                                                        * previous one if
                                                            *                                                                        * it is valid */
}

impl<const NUM_ENTRIES: usize> SmaTracker<NUM_ENTRIES> {
    /// Should never fail because time is positive and usize = u64
    pub fn time_to_epoch(&self, time: i64) -> Result<usize, OracleError> {
        try_convert::<i64, usize>(time / self.granularity)
    }

    pub fn initialize(&mut self, granularity: i64, threshold: u64) {
        self.threshold = threshold;
        self.granularity = granularity;
        self.current_epoch_is_valid = true;
    }

    pub fn add_datapoint(
        &mut self,
        last_two_times: (i64, i64),
        slot_gap: u64,
        price_to_be_added: i64,
        _last_two_confs: (u64, u64),
    ) -> Result<(), OracleError> {
        let epoch_0 = self.time_to_epoch(last_two_times.0)?;
        let epoch_1 = self.time_to_epoch(last_two_times.1)?;

        let datapoint_numerator = i128::from(slot_gap) * i128::from(price_to_be_added);
        let datapoint_denominator = slot_gap;

        self.current_epoch_denominator += datapoint_denominator;
        self.current_epoch_numerator += datapoint_numerator;
        self.current_epoch_is_valid =
            self.current_epoch_is_valid && datapoint_denominator <= self.threshold;

        // Next epoch transition
        for i in epoch_0..epoch_1 {
            self.conclude_epoch_and_initialize_next(i, datapoint_numerator, datapoint_denominator);
            self.current_epoch_is_valid = datapoint_denominator <= self.threshold;
        }
        Ok(())
    }

    // pub fn update_skipped_epochs(&mut self, epoch_0: usize, epoch_1 :usize,
    // datapoint_average:i128){

    //     for i in epoch_0+1..cmp::min(epoch_0 + NUM_ENTRIES, epoch_1){
    //         let index = i % NUM_ENTRIES;
    //         let prev_index = (i + (NUM_ENTRIES - 1)) % NUM_ENTRIES;

    //         self.running_sum_of_price_averages[index] =
    // self.running_sum_of_price_averages[prev_index] + datapoint_average;     }

    //     if epoch_1 > epoch_0 + NUM_ENTRIES{
    //         for i in 0..NUM_ENTRIES {
    //             let times_skipped = self.get_times_skipped(i, epoch_0, epoch_1);
    //             if times_skipped > 1 {
    //                 self.running_sum_of_price_averages[i] = self.running_sum_of_price_averages[i]
    //                 + datapoint_average * (times_skipped as i128 -1);
    //             }
    //         }
    //     }
    // }


    pub fn conclude_epoch_and_initialize_next(
        &mut self,
        epoch: usize,
        datapoint_numerator: i128,
        datapoint_denominator: u64,
    ) {
        let index = epoch % NUM_ENTRIES;
        let prev_index = (epoch + (NUM_ENTRIES - 1)) % NUM_ENTRIES;

        self.running_sum_of_price_averages[index] = self.running_sum_of_price_averages[prev_index]
            + self.current_epoch_numerator / i128::from(self.current_epoch_denominator);

        if self.current_epoch_is_valid {
            self.running_valid_entry_counter[index] =
                self.running_valid_entry_counter[prev_index] + 1;
        } else {
            self.running_valid_entry_counter[index] = self.running_valid_entry_counter[prev_index]
        };

        self.current_epoch_denominator = datapoint_denominator;
        self.current_epoch_numerator = datapoint_numerator;
    }

    // pub fn get_times_skipped(&self, i : usize, epoch_0 : usize, epoch_1 : usize) -> usize{
    //     let base = epoch_1 - epoch_0 / NUM_ENTRIES;
    //     let remainder_0 = epoch_0 % NUM_ENTRIES;
    //     let remainder_1 = epoch_1 % NUM_ENTRIES;
    //     let is_included = (remainder_0 < i && i < remainder_1) || (remainder_0 >= remainder_1) &&
    // (i < remainder_1 ) && (remainder_0 < i);     if is_included {
    //         return base;
    //     } else {
    //         return base +1;
    //     }
    // }
}


#[cfg(target_endian = "little")]
unsafe impl Zeroable for PriceAccountWrapper {
}

#[cfg(target_endian = "little")]
unsafe impl Pod for PriceAccountWrapper {
}

#[cfg(target_endian = "little")]
unsafe impl Zeroable for SmaTracker<48> {
}

#[cfg(target_endian = "little")]
unsafe impl Pod for SmaTracker<48> {
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
        TimeMachineWrapper,
    };
    use std::mem::size_of;
    #[test]
    ///test that the size defined in C matches that
    ///defined in Rust
    fn c_time_machine_size_is_correct() {
        assert_eq!(
        size_of::<TimeMachineWrapper>(),
        TIME_MACHINE_STRUCT_SIZE as usize,
        "expected TIME_MACHINE_STRUCT_SIZE ({}) in oracle.h to the same as the size of TimeMachineWrapper ({})",
        TIME_MACHINE_STRUCT_SIZE,
        size_of::<TimeMachineWrapper>()
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
