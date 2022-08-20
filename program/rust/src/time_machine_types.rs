use crate::c_oracle_header::{
    pc_price_t,
    PythAccount,
    EXTRA_PUBLISHER_SPACE,
    PC_ACCTYPE_PRICE,
    PC_PRICE_T_COMP_OFFSET,
};
use crate::error::OracleError;
use crate::utils::try_convert;
use bytemuck::{
    Pod,
    Zeroable,
};
use std::cmp::min;

//To make it easy to change the types to allow for more useful
//running sums if needed
type SignedTrackerRunningSum = i64;
type UnsignedTrackerRunningSum = u64;


#[repr(u8)]
#[derive(Debug, Copy, Clone, PartialEq, Eq)]
pub enum Status {
    Invalid = 0,
    Valid   = 1,
    Pending = 2,
}
// multiply by this to make up for errors that result when dividing integers
pub const SMA_TRACKER_PRECISION_MULTIPLIER: i64 = 10;
// Our computation off the SMA involves three averaging operations:
// 1. Averaging the two most recent prices, results in an error of at most 1 (we are rounding down)
// 2. Slot weighted Average (results in an error bounded to that of the previous step plus at most,
// one way to think about is that the error of the average is the average error of the entries plus
// the error computing the average) 3. Averages across entries (Ditto)
// This means that our error is bounded by 3
// So we multiply by 10, do the averaging, then divide by 10 and get the closes integer


mod track_helpers {
    use std::ops::{
        Add,
        Div,
        Mul,
    };


    use crate::error::OracleError;
    use crate::time_machine_types::SMA_TRACKER_PRECISION_MULTIPLIER;
    use crate::utils::try_convert;

    ///gets the index of the next entry
    pub fn get_next_entry(current_entry: usize, num_entries: usize) -> usize {
        (current_entry + 1) % num_entries
    }
    ///gets the index of the previous entry
    pub fn get_prev_entry(current_entry: usize, num_entries: usize) -> usize {
        (current_entry - 1) % num_entries
    }
    ///Given a time, return the entry of that time
    pub fn time_to_entry(
        time: i64,
        num_entries: usize,
        granularity: i64,
    ) -> Result<usize, OracleError> {
        Ok(try_convert::<i64, usize>(time / granularity)? % num_entries)
    }

    ///returns the remaining time before the next multiple of GRANULARITY
    pub fn get_time_to_entry_end(time: i64, granularity: i64) -> i64 {
        granularity - (time % granularity)
    }
    pub fn get_inflated_average<
        T: TryFrom<i64> + Mul<T, Output = T> + Add<T, Output = T> + Div<T, Output = T>,
    >(
        last_two_values: (T, T),
    ) -> Result<T, OracleError> {
        let (prev, current) = last_two_values;
        let inflated_prev = prev * try_convert::<i64, T>(SMA_TRACKER_PRECISION_MULTIPLIER)?;
        let inflated_current = current * try_convert::<i64, T>(SMA_TRACKER_PRECISION_MULTIPLIER)?;
        Ok((inflated_prev + inflated_current) / try_convert::<i64, T>(2)?)
    }
}
use track_helpers::*;

#[derive(Debug, Copy, Clone)]
#[repr(C)]
/// Represents a Simple Moving Average Tracker Tracker that has NUM_ENTRIES entries
/// each tracking time weighted sums for granularity seconds periods.
///The prices are assumed to be provided under some fixed point representation, and the computation
/// guarantees accuracy up to the last decimal digit in the fixed point representation.
/// Assumes threshold < granularity
/// Both threshold and granularity are set on initialization
pub struct SmaTracker<const NUM_ENTRIES: usize> {
    threshold:                                     u64, /* the maximum slot gap we are willing
                                                         * to accept */
    granularity:                                   i64, //the length of the time covered per entry
    current_entry_slot_accumulator:                u64, /* accumulates the number of slots that
                                                         * went into the
                                                         * weights of the current entry, reset
                                                         * to 0 before
                                                         * moving to the next one */
    current_entry_status:                          Status, /* the status of the current entry,
                                                            * INVALID if an update takes too
                                                            * long, PENDING if it is still being
                                                            * updated but the updates have been
                                                            * consistent so far. */
    current_entry_weighted_price_accumulator:      SignedTrackerRunningSum, /* accumulates
                                                                             * slot_delta *
                                                                             * (inflated_p1 +
                                                                             * inflated_p2) / 2
                                                                             * to compute
                                                                             * averages */
    current_entry_weighted_confidence_accumulator: UnsignedTrackerRunningSum, //ditto
    running_entry_prices:                          [SignedTrackerRunningSum; NUM_ENTRIES], /* A running sum of the slot weighted average of each entry. */
    running_entry_confidences:                     [UnsignedTrackerRunningSum; NUM_ENTRIES], //Ditto
    running_valid_entry_counter:                   [u64; NUM_ENTRIES], /* Each entry
                                                                        * increment the
                                                                        * counter of the
                                                                        * previous one if
                                                                        * it is valid */
}

impl<const NUM_ENTRIES: usize> SmaTracker<NUM_ENTRIES> {
    ///sets threshold and Granularity, and makes it so that the first produced
    /// entry is invalid
    pub fn initialize(&mut self, threshold: u64, granularity: i64) -> Result<(), OracleError> {
        //ensure that the first entry is invalid
        self.threshold = threshold;
        self.granularity = granularity;
        self.current_entry_status = Status::Invalid;
        Ok(())
    }

    ///Adds the average of the last two price data points
    /// to the tracker, weights them by the slot gap
    pub fn add_price(
        &mut self,
        prev_time: i64,
        current_time: i64,
        last_two_prices: (i64, i64),
        last_two_confs: (u64, u64),
        slot_gap: u64,
    ) -> Result<(), OracleError> {
        let prev_entry = time_to_entry(prev_time, NUM_ENTRIES, self.granularity)?;
        let current_entry = time_to_entry(current_time, NUM_ENTRIES, self.granularity)?;
        let num_skipped_entries = current_entry - prev_entry;

        let mut entry_to_finalize = prev_entry;
        for _ in 0..min(num_skipped_entries - 1, NUM_ENTRIES) {
            self.move_to_next_entry(self.threshold, entry_to_finalize)?;
            entry_to_finalize = get_next_entry(entry_to_finalize, NUM_ENTRIES);
        }

        self.update_entry_price(last_two_prices, last_two_confs, slot_gap)
    }
    ///updates the current entry with the given price, confidence,
    /// and sets it's validity according to slot gap
    fn update_entry_price(
        &mut self,
        last_two_prices: (i64, i64),
        last_two_confs: (u64, u64),
        slot_gap: u64,
    ) -> Result<(), OracleError> {
        //multiply by precision multiplier so that our entry aggregated can be rounded to have
        //the same accuracy as user input
        //update slot accumulator
        self.current_entry_slot_accumulator += slot_gap;
        //update price accumulator
        let inflated_avg_price = get_inflated_average(last_two_prices)?;
        self.current_entry_weighted_price_accumulator += try_convert::<i64, SignedTrackerRunningSum>(
            try_convert::<u64, i64>(slot_gap)? * inflated_avg_price,
        )?;
        //update confidence accumulator
        let inflated_avg_conf = get_inflated_average(last_two_confs)?;
        self.current_entry_weighted_confidence_accumulator +=
            try_convert::<u64, UnsignedTrackerRunningSum>(
                try_convert::<u64, u64>(slot_gap)? * inflated_avg_conf,
            )?;
        //update entry validity
        if slot_gap >= self.threshold {
            self.current_entry_status = Status::Invalid;
        }
        Ok(())
    }
    /// determines the status of the current next entry based on slot_gap
    /// and computes the average of the current entry, so that it is added to the previous one
    fn move_to_next_entry(&mut self, slot_gap: u64, entry: usize) -> Result<(), OracleError> {
        let prev_entry = get_prev_entry(entry, NUM_ENTRIES);
        if self.current_entry_slot_accumulator != 0 {
            //compute the current entry's price
            self.running_entry_prices[entry] += self.current_entry_weighted_price_accumulator
                / try_convert::<u64, SignedTrackerRunningSum>(self.current_entry_slot_accumulator)?;
            //compute the current entry's confidence
            self.running_entry_confidences[entry] += self
                .current_entry_weighted_confidence_accumulator
                / try_convert::<u64, UnsignedTrackerRunningSum>(
                    self.current_entry_slot_accumulator,
                )?;
        } else {
            self.running_entry_prices[entry] = self.running_entry_prices[prev_entry];
            self.running_entry_confidences[entry] = self.running_entry_confidences[prev_entry];
            self.current_entry_status = Status::Invalid;
        }
        //check current entry validity
        self.running_valid_entry_counter[entry] =
            if self.current_entry_status == Status::Pending && slot_gap < self.threshold {
                self.running_valid_entry_counter[prev_entry] + 1
            } else {
                self.running_valid_entry_counter[prev_entry]
            };


        //setup working variables for the next entry
        self.current_entry_slot_accumulator = 0;
        self.current_entry_weighted_confidence_accumulator = 0;
        self.current_entry_weighted_price_accumulator = 0;
        self.current_entry_status = if slot_gap < self.threshold {
            Status::Pending
        } else {
            Status::Invalid
        };
        Ok(())
    }
    ///invalidate everything as a panic button
    pub fn invalidate_state(&mut self) {
        self.current_entry_slot_accumulator = 0;
        self.current_entry_weighted_confidence_accumulator = 0;
        self.current_entry_weighted_price_accumulator = 0;
        self.current_entry_status = Status::Invalid;
        for validity in self.running_valid_entry_counter.iter_mut() {
            *validity = 0;
        }
    }
    #[allow(dead_code)]
    ///gets the average of all the entries between start and end offsets (half_open)
    /// where offsets are the offsets from the current entry
    pub fn get_sma_entries_ago(
        &self,
        start_offset: usize,
        end_offset: usize,
        last_update_time: i64,
    ) -> Result<(i64, u64, Status), OracleError> {
        if start_offset < end_offset || start_offset > NUM_ENTRIES - 1 {
            return Err(OracleError::InvalidTimeMachineRequest);
        }
        let current_entry = time_to_entry(last_update_time, NUM_ENTRIES, self.granularity)?;
        let pre_start_entry = (current_entry + 2 * NUM_ENTRIES - start_offset - 1) % NUM_ENTRIES;
        let pre_end_entry = (current_entry + 2 * NUM_ENTRIES - end_offset - 1) % NUM_ENTRIES;
        let num_avg_entries = end_offset - start_offset;
        let avg_price = (self.running_entry_prices[pre_end_entry]
            - self.running_entry_prices[pre_start_entry])
            / try_convert::<usize, i64>(num_avg_entries)?;
        let avg_confidence = (self.running_entry_confidences[pre_end_entry]
            - self.running_entry_confidences[pre_start_entry])
            / try_convert::<usize, u64>(num_avg_entries)?;
        let validity = if (self.running_valid_entry_counter[pre_end_entry]
            - self.running_valid_entry_counter[pre_start_entry])
            == try_convert::<usize, u64>(num_avg_entries)?
        {
            Status::Valid
        } else {
            Status::Invalid
        };
        Ok((avg_price, avg_confidence, validity))
    }
}


#[derive(Debug, Copy, Clone)]
#[repr(C)]
/// A Tracker that has NUM_ENTRIES entries
/// each tracking the last tick (price and confidence) before every granularity seconds
/// where granularity is set on initialization
pub struct TickTracker<const NUM_ENTRIES: usize> {
    threshold:      i64,
    granularity:    i64,
    prices:         [SignedTrackerRunningSum; NUM_ENTRIES], /* price running time
                                                             * weighted sums */
    confidences:    [UnsignedTrackerRunningSum; NUM_ENTRIES], /* confidence running
                                                               * time
                                                               * weighted sums */
    entry_validity: [Status; NUM_ENTRIES], /* entry validity, INVALID if the last tick in an
                                            * entry is too far from the end, PENDING if it is
                                            * the entry is still being updated but the current
                                            * tick is close enough, VALID if the entry is no
                                            * longer being updated and the last tick is
                                            * sufficiently close */
}


impl<const NUM_ENTRIES: usize> TickTracker<NUM_ENTRIES> {
    pub fn initialize(&mut self, threshold: i64, granularity: i64) -> Result<(), OracleError> {
        self.threshold = threshold;
        self.granularity = granularity;
        Ok(())
    }

    ///add a new price to the tracker
    pub fn add_price(
        &mut self,
        prev_time: i64,
        current_time: i64,
        current_price: i64,
        current_conf: u64,
    ) -> Result<(), OracleError> {
        //close current entry
        let current_entry = time_to_entry(current_time, NUM_ENTRIES, self.granularity)?;
        let prev_entry = time_to_entry(prev_time, NUM_ENTRIES, self.granularity)?;
        if current_entry != prev_entry && self.entry_validity[prev_entry] == Status::Pending {
            self.entry_validity[prev_entry] = Status::Valid;
        }

        //invalidate intermediate entries
        let num_skipped_entries = current_entry - prev_entry;
        let mut entry = prev_entry;
        for _ in 0..min(num_skipped_entries, NUM_ENTRIES) {
            entry = get_next_entry(entry, NUM_ENTRIES);
            self.entry_validity[current_entry] = Status::Invalid;
        }

        //update current tick
        self.prices[current_entry] = current_price;
        self.confidences[current_entry] = current_conf;
        self.entry_validity[current_entry] =
            if get_time_to_entry_end(current_time, self.granularity) >= self.threshold {
                Status::Invalid
            } else {
                Status::Pending
            };
        Ok(())
    }
    ///invalidate everything as a panic button
    pub fn invalidate_state(&mut self) {
        for validity in self.entry_validity.iter_mut() {
            *validity = Status::Invalid;
        }
    }
}

const THIRTY_MINUTES: i64 = 30 * 60;
const TEN_HOURS: usize = 10 * 60 * 60;
const TWENTY_SECONDS: u64 = 20;
#[derive(Debug, Clone, Copy)]
#[repr(C)]
/// this wraps multiple SMA and tick trackers, and includes all the state
/// used by the time machine
pub struct TimeMachineWrapper {
    sma_tracker:  SmaTracker<{ TEN_HOURS / (THIRTY_MINUTES as usize) }>,
    tick_tracker: TickTracker<{ TEN_HOURS / (THIRTY_MINUTES as usize) }>,
}

impl TimeMachineWrapper {
    pub fn initialize(&mut self) -> Result<(), OracleError> {
        self.sma_tracker
            .initialize(TWENTY_SECONDS, THIRTY_MINUTES)?;
        self.tick_tracker
            .initialize(try_convert(TWENTY_SECONDS)?, THIRTY_MINUTES)?;
        Ok(())
    }

    pub fn add_price(
        &mut self,
        prev_time: i64,
        current_time: i64,
        last_two_prices: (i64, i64),
        last_two_confs: (u64, u64),
        slot_gap: u64,
    ) -> Result<(), OracleError> {
        if self
            .sma_tracker
            .add_price(
                prev_time,
                current_time,
                last_two_prices,
                last_two_confs,
                slot_gap,
            )
            .is_err()
        {
            self.sma_tracker.invalidate_state();
        }
        if self
            .tick_tracker
            .add_price(prev_time, current_time, last_two_prices.1, last_two_confs.1)
            .is_err()
        {
            self.tick_tracker.invalidate_state();
        }
        Ok(())
    }
}

#[derive(Copy, Clone)]
#[repr(C)]
/// wraps everything stored in a price account
pub struct PriceAccountWrapper {
    //an instance of the c price_t type
    pub price_data:            pc_price_t,
    //space for more publishers
    pub extra_publisher_space: [u8; EXTRA_PUBLISHER_SPACE as usize],
    //TimeMachine
    pub time_machine:          TimeMachineWrapper,
}
impl PriceAccountWrapper {
    pub fn initialize_time_machine(&mut self) -> Result<(), OracleError> {
        self.time_machine.initialize()
    }

    pub fn add_price_to_time_machine(&mut self) -> Result<(), OracleError> {
        //this would work on the first add_price as well, as the first entry is always invalid, so
        // even though we would probabpy end up overflowing the first number that should not corrupt
        // any valid data
        self.time_machine.add_price(
            self.price_data.prev_timestamp_,
            self.price_data.timestamp_,
            (self.price_data.prev_price_, self.price_data.agg_.price_),
            (self.price_data.prev_conf_, self.price_data.agg_.conf_),
            self.price_data.agg_.pub_slot_ - self.price_data.prev_slot_,
        )
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
        TimeMachineWrapper,
    };
    use std::mem::size_of;
    #[test]
    ///test that the size defined in C matches that
    ///defined in Rust
    fn c_time_machine_size_is_correct() {
        assert_eq!(
        size_of::<TimeMachineWrapper>(),
        TIME_MACHINE_STRUCT_SIZE.try_into().unwrap(),
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
        PRICE_ACCOUNT_SIZE.try_into().unwrap(),
        "expected PRICE_ACCOUNT_SIZE ({}) in oracle.h to the same as the size of PriceAccountWrapper ({})",
        PRICE_ACCOUNT_SIZE,
        size_of::<PriceAccountWrapper>()
    );
    }
}
