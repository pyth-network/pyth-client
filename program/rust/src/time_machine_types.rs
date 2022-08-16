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
use std::cmp;

//To make it easy to change the types to allow for more usefull
//running sums if needed
use {
    i64 as SignedTrackerRunningSum,
    u64 as UnsignedTrackerRunningSum,
};
//multiply by this to make up for errors that result when dividing integers
pub const SMA_TRACKER_PRECISION_MULTIPLIER: i64 = 10;


pub trait Tracker<const GRANULARITY: i64, const NUM_ENTRIES: usize, const THRESHOLD: i64> {
    ///initializes a zero initialized tracker
    fn initialize(&mut self) -> Result<(), OracleError>;

    ///add a new price to the tracker
    fn add_price(
        &mut self,
        prev_time: i64,
        prev_price: i64,
        prev_conf: u64,
        current_time: i64,
        current_price: i64,
        current_conf: u64,
    ) -> Result<(), OracleError>;
    ///gets the index of the next entry
    fn get_next_entry(current_entry: usize) -> usize {
        (current_entry + 1) % NUM_ENTRIES
    }
    ///Given a time, return the entry of that time
    fn time_to_entry(time: i64) -> Result<usize, OracleError> {
        Ok(try_convert::<i64, usize>(time / GRANULARITY)? % NUM_ENTRIES)
    }
    ///returns the remining time before the next multiple of GRANULARITY
    fn get_time_to_entry_end(time: i64) -> i64 {
        (GRANULARITY - (time % GRANULARITY)) % GRANULARITY
    }
}

#[derive(Debug, Copy, Clone)]
#[repr(C)]
/// Represents a Simple Moving Average Tracker Tracker that has NUM_ENTRIES entries
/// each tracking time weighted sums for GRANULARITY seconds periods.
///The prices are assumed to be provided under some fixed point representation, and the computation
/// gurantees accuracy up to the last decimal digit in the fixed point representation.
/// Assumes THRESHOLD < GRANULARITY
pub struct SmaTracker<const GRANULARITY: i64, const NUM_ENTRIES: usize, const THRESHOLD: i64> {
    valid_entry_counter:        u64, // is incremented everytime we add a valid entry
    max_update_time_gap:        i64, // maximum time between two updates in the current entry.
    running_price:              [SignedTrackerRunningSum; NUM_ENTRIES], /* price running time
                                      * weighted sums */
    running_confidence:         [UnsignedTrackerRunningSum; NUM_ENTRIES], /* confidence running
                                                                           * time
                                                                           * weighted sums */
    max_observed_entry_abs_val: u64, /* used to ensure that there are no overflows when
                                      * looking at running sum diffs */
    entry_validity:             [u8; NUM_ENTRIES], //entry validity
}

impl<const GRANULARITY: i64, const NUM_ENTRIES: usize, const THRESHOLD: i64>
    SmaTracker<GRANULARITY, NUM_ENTRIES, THRESHOLD>
{
    ///invalidates current_entry, and increments self.current_entry num_entries times
    fn invalidate_following_entries(&mut self, num_entries: usize, current_entry: usize) {
        let mut entry = current_entry;
        for _ in 0..num_entries {
            self.entry_validity[entry] = 0;
            entry = Self::get_next_entry(entry);
        }
    }
    /// returns a weighted average of v with weights w
    fn weighted_average<
        T: std::ops::Mul<Output = T>
            + std::ops::Add<Output = T>
            + std::ops::Div<Output = T>
            + std::marker::Copy,
    >(
        w1: T,
        v1: T,
        w2: T,
        v2: T,
    ) -> Result<T, OracleError> {
        Ok((w1 * v1 + w2 * v2) / (w1 + w2))
    }
    ///Adds price and confidence each multiplied by update_time
    /// to the entry
    fn add_price_to_entry(
        &mut self,
        prev_price: i64,
        prev_conf: u64,
        current_price: i64,
        current_conf: u64,
        update_time: i64,
        entry: usize,
    ) -> Result<(), OracleError> {
        let avg_price = (prev_price + current_price) / 2;
        let avg_conf = (prev_conf + current_conf) / 2;
        self.running_price[entry] +=
            try_convert::<i64, SignedTrackerRunningSum>(update_time * avg_price)?;
        self.running_confidence[entry] += try_convert::<u64, UnsignedTrackerRunningSum>(
            try_convert::<i64, u64>(update_time)? * avg_conf,
        )?;
        //since price is always greater than confidence, it is sufficient to look at the
        // price we also look at the average because it is the value that
        // gets multiplied with the number of seconds
        self.max_observed_entry_abs_val =
            cmp::max(try_convert(avg_price)?, self.max_observed_entry_abs_val);
        Ok(())
    }
}

impl<const GRANULARITY: i64, const NUM_ENTRIES: usize, const THRESHOLD: i64>
    Tracker<GRANULARITY, NUM_ENTRIES, THRESHOLD>
    for SmaTracker<GRANULARITY, NUM_ENTRIES, THRESHOLD>
{
    fn initialize(&mut self) -> Result<(), OracleError> {
        //ensure that the first entry is invalid
        self.max_update_time_gap = THRESHOLD;
        Ok(())
    }

    ///add a new price to the tracker
    fn add_price(
        &mut self,
        prev_time: i64,
        prev_price: i64,
        prev_conf: u64,
        current_time: i64,
        current_price: i64,
        current_conf: u64,
    ) -> Result<(), OracleError> {
        //multiply by precision multiplier so that our entry aggregated can be rounded to have
        //the same accuracy as user input
        let inflated_prev_price = prev_price * SMA_TRACKER_PRECISION_MULTIPLIER;
        let inflated_current_price = current_price * SMA_TRACKER_PRECISION_MULTIPLIER;
        let inflated_prev_conf =
            prev_conf * try_convert::<i64, u64>(SMA_TRACKER_PRECISION_MULTIPLIER)?;
        let inflated_current_conf =
            current_conf * try_convert::<i64, u64>(SMA_TRACKER_PRECISION_MULTIPLIER)?;

        let update_time = current_time - prev_time;
        self.max_update_time_gap = cmp::max(self.max_update_time_gap, update_time);

        let prev_entry = Self::time_to_entry(prev_time)?;
        let current_entry = Self::time_to_entry(current_time)?;
        let num_skipped_entries = current_entry - prev_entry;
        //update entries as needed
        match current_entry - prev_entry {
            0 => {
                self.add_price_to_entry(
                    inflated_prev_price,
                    inflated_prev_conf,
                    inflated_current_price,
                    inflated_current_conf,
                    update_time,
                    prev_entry,
                )?;
            }
            1 => {
                //initialize next entry to be the same as the current one

                self.running_price[current_entry] = self.running_price[prev_entry];
                self.running_confidence[current_entry] = self.running_confidence[prev_entry];

                //update price
                let time_to_entry_end = Self::get_time_to_entry_end(prev_time);
                let time_after_entry_end = update_time - time_to_entry_end;
                let entry_end_price = Self::weighted_average(
                    time_after_entry_end,
                    inflated_prev_price,
                    time_to_entry_end,
                    inflated_current_price,
                )?;
                let entry_end_conf = Self::weighted_average::<u64>(
                    try_convert::<i64, u64>(time_after_entry_end)?,
                    inflated_prev_conf,
                    try_convert::<i64, u64>(time_to_entry_end)?,
                    inflated_current_conf,
                )?;

                self.add_price_to_entry(
                    inflated_prev_price,
                    inflated_prev_conf,
                    entry_end_price,
                    entry_end_conf,
                    time_to_entry_end,
                    prev_entry,
                )?;

                //update current entry validity
                self.entry_validity[prev_entry] = if self.max_update_time_gap >= THRESHOLD {
                    0
                } else {
                    self.valid_entry_counter += 1;
                    1
                };

                //update max_update_time
                self.max_update_time_gap = update_time;


                self.add_price_to_entry(
                    inflated_prev_price,
                    inflated_prev_conf,
                    inflated_current_price,
                    inflated_current_conf,
                    update_time,
                    current_entry,
                )?;
            }
            _ => {
                //invalidate all the entries in your way
                //this is ok because THRESHOLD < GranulARity
                self.invalidate_following_entries(
                    cmp::min(num_skipped_entries, NUM_ENTRIES),
                    prev_entry,
                );
                self.max_update_time_gap = update_time;
            }
        }
        Ok(())
    }
}


#[derive(Debug, Copy, Clone)]
#[repr(C)]
/// A Tracker that has NUM_ENTRIES entries
/// each tracking the last tick (price and confidence) before every GRANULARITY seconds.
pub struct TickTracker<const GRANULARITY: i64, const NUM_ENTRIES: usize, const THRESHOLD: i64> {
    prices:         [SignedTrackerRunningSum; NUM_ENTRIES], /* price running time
                                                             * weighted sums */
    confidences:    [UnsignedTrackerRunningSum; NUM_ENTRIES], /* confidence running
                                                               * time
                                                               * weighted sums */
    entry_validity: [u8; NUM_ENTRIES], //entry validity
}


impl<const GRANULARITY: i64, const NUM_ENTRIES: usize, const THRESHOLD: i64>
    TickTracker<GRANULARITY, NUM_ENTRIES, THRESHOLD>
{
    ///invalidates current_entry, and increments self.current_entry num_entries times
    fn invalidate_following_entries(&mut self, num_entries: usize, current_entry: usize) {
        let mut entry = current_entry;
        for _ in 0..num_entries {
            self.entry_validity[entry] = 0;
            entry = Self::get_next_entry(entry);
        }
    }
}


impl<const GRANULARITY: i64, const NUM_ENTRIES: usize, const THRESHOLD: i64>
    Tracker<GRANULARITY, NUM_ENTRIES, THRESHOLD>
    for TickTracker<GRANULARITY, NUM_ENTRIES, THRESHOLD>
{
    fn initialize(&mut self) -> Result<(), OracleError> {
        Ok(())
    }

    ///add a new price to the tracker
    fn add_price(
        &mut self,
        prev_time: i64,
        prev_price: i64,
        prev_conf: u64,
        current_time: i64,
        current_price: i64,
        current_conf: u64,
    ) -> Result<(), OracleError> {
        let prev_entry = Self::time_to_entry(prev_time)?;
        let current_entry = Self::time_to_entry(current_time)?;
        let num_skipped_entries = current_entry - prev_entry;
        match num_skipped_entries {
            0 => {
                self.prices[current_entry] = current_price;
                self.confidences[current_entry] = current_conf;
            }
            1 => {
                self.prices[prev_entry] = prev_price;
                self.confidences[prev_entry] = prev_conf;
                self.entry_validity[prev_entry] =
                    if Self::get_time_to_entry_end(prev_time) >= THRESHOLD {
                        0
                    } else {
                        1
                    };
                self.prices[current_entry] = current_price;
                self.confidences[current_entry] = current_conf;
            }
            _ => {
                //invalidate all the entries in your way
                //this is ok because THRESHOLD < GranulARity
                self.prices[prev_entry] = prev_price;
                self.confidences[prev_entry] = prev_conf;
                self.entry_validity[prev_entry] =
                    if Self::get_time_to_entry_end(prev_time) >= THRESHOLD {
                        0
                    } else {
                        1
                    };

                self.invalidate_following_entries(
                    cmp::min(num_skipped_entries - 1, NUM_ENTRIES),
                    Self::get_next_entry(prev_entry),
                );
                self.prices[current_entry] = current_price;
                self.confidences[current_entry] = current_conf;
            }
        }

        Ok(())
    }
}

const THIRTY_MINUTES: i64 = 30 * 60;
const TEN_HOURS: usize = 10 * 60 * 60;
const TWENTY_SECONDS: i64 = 20;
#[derive(Debug, Clone, Copy)]
#[repr(C)]
/// this wraps multiple SMA and tick trackers, and includes all the state
/// used by the time machine
pub struct TimeMachineWrapper {
    sma_tracker:
        SmaTracker<THIRTY_MINUTES, { TEN_HOURS / (THIRTY_MINUTES as usize) }, TWENTY_SECONDS>,
    tick_tracker:
        TickTracker<THIRTY_MINUTES, { TEN_HOURS / (THIRTY_MINUTES as usize) }, TWENTY_SECONDS>,
}

impl TimeMachineWrapper {
    pub fn initialize(&mut self) -> Result<(), OracleError> {
        self.sma_tracker.initialize()?;
        self.tick_tracker.initialize()?;
        Ok(())
    }

    pub fn add_price(
        &mut self,
        prev_time: i64,
        prev_price: i64,
        prev_conf: u64,
        current_time: i64,
        current_price: i64,
        current_conf: u64,
    ) -> Result<(), OracleError> {
        self.sma_tracker.add_price(
            prev_time,
            prev_price,
            prev_conf,
            current_time,
            current_price,
            current_conf,
        )?;
        self.tick_tracker.add_price(
            prev_time,
            prev_price,
            prev_conf,
            current_time,
            current_price,
            current_conf,
        )?;
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
            self.price_data.prev_price_,
            self.price_data.prev_conf_,
            self.price_data.timestamp_,
            self.price_data.agg_.price_,
            self.price_data.agg_.conf_,
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
