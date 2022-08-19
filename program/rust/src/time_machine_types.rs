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
type SignedTrackerRunningSum = i64;
type UnsignedTrackerRunningSum = u64;


#[repr(u8)]
#[derive(Debug, Copy, Clone, PartialEq)]
enum Status {
    Invalid = 0,
    Valid   = 1,
    Pending = 2,
}
// multiply by this to make up for errors that result when dividing integers
pub const SMA_TRACKER_PRECISION_MULTIPLIER: i64 = 10;
// Our computation off the SMA involves three averaging operations:
// 1. Averaging the two most recent prices, results in an error of atmost 1 (we are rounding down)
// 2. Slot weighted Average (results in an error bounded to that of the previous step plus at most,
// one way to think about is that the error of the average is the average error of the entries plus
// the error computing the average) 3. Averages accrosss entries (Ditto)
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
    ///gets the index of the previou entry
    pub fn get_prev_entry(current_entry: usize, num_entries: usize) -> usize {
        (current_entry - 1) % num_entries
    }
    ///Given a time, return the entry of that time
    pub fn time_to_entry(
        time: i64,
        num_entries: usize,
        granuality: i64,
    ) -> Result<usize, OracleError> {
        Ok(try_convert::<i64, usize>(time / granuality)? % num_entries)
    }

    ///returns the remining time before the next multiple of GRANULARITY
    pub fn get_time_to_entry_end(time: i64, granuality: i64) -> i64 {
        granuality - (time % granuality)
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
/// each tracking time weighted sums for GRANULARITY seconds periods.
///The prices are assumed to be provided under some fixed point representation, and the computation
/// gurantees accuracy up to the last decimal digit in the fixed point representation.
/// Assumes THRESHOLD < GRANULARITY
pub struct SmaTracker<const NUM_ENTRIES: usize> {
    threshold:                      u64, //the maximum slot gap we are willing to accept
    granularity:                    i64, //the length of the time covered per entry
    current_entry_slot_accumelator: u64, /* accumelates the number of slots that went into the
                                          * weights of the current entry, reset to 0 before
                                          * moving to the next one */
    current_entry_status:           Status, //the status of the current entry
    running_entry_prices:           [SignedTrackerRunningSum; NUM_ENTRIES], /* A running sum of the slot weighted average of each entry. (except for current entry, which is a working space for computing the current slot weighted average) */
    running_entry_confidences:      [UnsignedTrackerRunningSum; NUM_ENTRIES], //Ditto
    running_valid_entry_counter:    [u64; NUM_ENTRIES],                     /* Each entry
                                                                             * increment the
                                                                             * counter of the
                                                                             * previous one if
                                                                             * it is valid */
}

impl<const NUM_ENTRIES: usize> SmaTracker<NUM_ENTRIES> {
    ///sets threshold and Granuality, and makes it so that the first produced
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
        //multiply by precision multiplier so that our entry aggregated can be rounded to have
        //the same accuracy as user input
        let inflated_avg_price = get_inflated_average(last_two_prices)?;
        let inflated_avg_conf = get_inflated_average(last_two_confs)?;
        let prev_entry = time_to_entry(prev_time, NUM_ENTRIES, self.granularity)?;
        let current_entry = time_to_entry(current_time, NUM_ENTRIES, self.granularity)?;
        let num_skipped_entries = current_entry - prev_entry;
        //check for overflow, and set things up accordingly

        //set up to update current_entry
        match num_skipped_entries {
            0 => {}
            1 => {
                self.move_to_next_entry(slot_gap, prev_entry)?;
            }
            _ => {
                //invalidate all the entries in your way
                //this is ok because THRESHOLD < GranulARity
                self.invalidate_following_entries(prev_entry, num_skipped_entries)?;
            }
        }
        self.update_entry_price(
            inflated_avg_price,
            inflated_avg_conf,
            slot_gap,
            current_entry,
        )
    }
    ///updates the current entry with the given price, confidence,
    /// and sets it's validity according to slot gap
    fn update_entry_price(
        &mut self,
        avg_price: i64,
        avg_conf: u64,
        slot_gap: u64,
        entry: usize,
    ) -> Result<(), OracleError> {
        self.current_entry_slot_accumelator += slot_gap;
        self.running_entry_prices[entry] += try_convert::<i64, SignedTrackerRunningSum>(
            try_convert::<u64, i64>(slot_gap)? * avg_price,
        )?;
        self.running_entry_confidences[entry] += try_convert::<u64, UnsignedTrackerRunningSum>(
            try_convert::<u64, u64>(slot_gap)? * avg_conf,
        )?;
        if slot_gap >= self.threshold {
            self.current_entry_status = Status::Invalid;
        }
        Ok(())
    }
    /// determines the status of the current next entry based on slot_gap
    /// and computes the average of the current entry, so that it is added to the previous one
    fn move_to_next_entry(&mut self, slot_gap: u64, entry: usize) -> Result<(), OracleError> {
        let prev_entry = get_prev_entry(entry, NUM_ENTRIES);
        let next_entry = get_next_entry(entry, NUM_ENTRIES);
        //setup the current entry price
        self.running_entry_prices[entry] /=
            try_convert::<u64, SignedTrackerRunningSum>(self.current_entry_slot_accumelator)?;
        self.running_entry_prices[entry] += self.running_entry_prices[prev_entry];
        self.running_entry_confidences[entry] /=
            try_convert::<u64, UnsignedTrackerRunningSum>(self.current_entry_slot_accumelator)?;
        self.running_entry_confidences[entry] += self.running_entry_confidences[prev_entry];

        //check current entry validity
        self.running_valid_entry_counter[entry] =
            if self.current_entry_status == Status::Pending && slot_gap < self.threshold {
                self.running_valid_entry_counter[prev_entry] + 1
            } else {
                self.running_valid_entry_counter[prev_entry]
            };

        //setup the following entry
        self.current_entry_slot_accumelator = 0;
        self.current_entry_status = if slot_gap < self.threshold {
            Status::Pending
        } else {
            Status::Invalid
        };
        self.running_entry_confidences[next_entry] = 0;
        self.running_entry_prices[next_entry] = 0;
        self.running_valid_entry_counter[next_entry] = 0;
        Ok(())
    }
    ///Invalidates the given number of entries starting from and including
    /// the first one
    fn invalidate_following_entries(
        &mut self,
        entry: usize,
        num_invalid: usize,
    ) -> Result<(), OracleError> {
        let entries_to_invalidate = cmp::min(num_invalid, NUM_ENTRIES);
        let mut current_entry = entry;
        //each call to move_next_entry invalidates two entries that is why we subtract one
        for _ in 0..entries_to_invalidate - 1 {
            //we assume that entries must be invaldiated whenever  block is skipped
            self.move_to_next_entry(self.threshold, current_entry)?;
            current_entry = get_next_entry(current_entry, NUM_ENTRIES);
        }
        Ok(())
    }
}


#[derive(Debug, Copy, Clone)]
#[repr(C)]
/// A Tracker that has NUM_ENTRIES entries
/// each tracking the last tick (price and confidence) before every GRANULARITY seconds.
pub struct TickTracker<const NUM_ENTRIES: usize> {
    threshold:      i64,
    granularity:    i64,
    prices:         [SignedTrackerRunningSum; NUM_ENTRIES], /* price running time
                                                             * weighted sums */
    confidences:    [UnsignedTrackerRunningSum; NUM_ENTRIES], /* confidence running
                                                               * time
                                                               * weighted sums */
    entry_validity: [Status; NUM_ENTRIES], //entry validity
}


impl<const NUM_ENTRIES: usize> TickTracker<NUM_ENTRIES> {
    ///invalidates current_entry, and increments self.current_entry num_entries times
    fn invalidate_following_entries(&mut self, num_entries: usize, current_entry: usize) {
        let mut entry = current_entry;
        for _ in 0..num_entries {
            self.entry_validity[entry] = Status::Invalid;
            entry = get_next_entry(entry, NUM_ENTRIES);
        }
    }

    fn initialize(&mut self, threshold: i64, granularity: i64) -> Result<(), OracleError> {
        self.threshold = threshold;
        self.granularity = granularity;
        Ok(())
    }

    ///add a new price to the tracker
    fn add_price(
        &mut self,
        prev_time: i64,
        prev_price: i64,
        prev_conf: u64,
        current_time: i64,
    ) -> Result<(), OracleError> {
        let prev_entry = time_to_entry(prev_time, NUM_ENTRIES, self.granularity)?;
        let current_entry = time_to_entry(current_time, NUM_ENTRIES, self.granularity)?;
        let num_skipped_entries = current_entry - prev_entry;

        // unlike SMAs, first entry here can be valid
        self.prices[prev_entry] = prev_price;
        self.confidences[prev_entry] = prev_conf;

        //update validity if it is time to do so
        self.entry_validity[prev_entry] = if get_time_to_entry_end(prev_time, self.granularity)
            >= self.threshold
            || num_skipped_entries == 0
        {
            Status::Invalid
        } else {
            Status::Valid
        };

        //invalidate skipped entries if any
        self.invalidate_following_entries(
            cmp::min(num_skipped_entries, NUM_ENTRIES),
            get_next_entry(prev_entry, NUM_ENTRIES),
        );
        //We do not need to store thre current_price or conf, they will be stored on the next
        //price send. However, we can use to interpolate the border price if we wish to do so.
        Ok(())
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
        self.sma_tracker.add_price(
            prev_time,
            current_time,
            last_two_prices,
            last_two_confs,
            slot_gap,
        )?;
        self.tick_tracker.add_price(
            prev_time,
            last_two_prices.0,
            last_two_confs.0,
            current_time,
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
