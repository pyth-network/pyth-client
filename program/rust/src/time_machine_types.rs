use crate::c_oracle_header::{
    PriceAccount,
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
        self.time_machine.initialize(THIRTY_MINUTES, TWENTY_SECONDS);
        Ok(())
    }

    pub fn add_price_to_time_machine(&mut self) -> Result<(), OracleError> {
        self.time_machine.add_datapoint(
            (self.price_data.prev_timestamp_, self.price_data.timestamp_),
            (self.price_data.prev_slot_, self.price_data.last_slot_),
            (self.price_data.prev_price_, self.price_data.agg_.price_),
            (0, 0),
        )?;
        Ok(())
    }
}

const TWENTY_SECONDS: u64 = 20;
const THIRTY_MINUTES: i64 = 30 * 60;

#[repr(u8)]
#[derive(Debug, Copy, Clone, PartialEq, Eq)]
pub enum Status {
    Invalid = 0,
    Valid   = 1,
}

#[derive(Debug, Copy, Clone)]
#[repr(C)]
/// Represents a Simple Moving Average Tracker Tracker that has NUM_ENTRIES entries
/// each tracking time weighted sums for granularity seconds periods.
///The prices are assumed to be provided under some fixed point representation, and the computation
/// guarantees accuracy up to the last decimal digit in the fixed point representation.
/// Assumes threshold < granularity
/// Both threshold and granularity are set on initialization
pub struct SmaTracker<const NUM_ENTRIES: usize> {
    pub threshold:                                u64, /* the maximum slot gap we are willing
                                                        * to accept */
    pub granularity:                              i64, //the length of the time covered per entry
    pub current_entry_slot_accumulator:           u64, /* accumulates the number of slots that
                                                        * went into the
                                                        * weights of the current entry, reset
                                                        * to 0 before
                                                        * moving to the next one */
    pub current_entry_status:                     Status, /* the status of the current entry,
                                                           * INVALID if an update takes too
                                                           * long, PENDING if it is still being
                                                           * updated but the updates have been
                                                           * consistent so far. */
    pub current_entry_weighted_price_accumulator: i128, /* accumulates
                                                         * slot_delta *
                                                         * (inflated_p1 +
                                                         * inflated_p2) / 2
                                                         * to compute
                                                         * averages */
    pub running_entry_prices:                     [i128; NUM_ENTRIES], /* A running sum of the
                                                                        * slot weighted average
                                                                        * of each entry. */
    pub running_valid_entry_counter:              [u64; NUM_ENTRIES], /* Each entry
                                                                       * increment the
                                                                       * counter of the
                                                                       * previous one if
                                                                       * it is valid */
}

impl<const NUM_ENTRIES: usize> SmaTracker<NUM_ENTRIES> {
    /// Should never fail because time is positive and usize = u64
    pub fn time_to_epoch(&self, time: i64) -> Result<usize, OracleError> {
        try_convert::<i64, usize>(time / self.granularity)
    }

    pub fn initialize(&mut self, granularity: i64, threshold: u64) {
        self.threshold = threshold;
        self.granularity = granularity;
    }

    pub fn add_datapoint(
        &mut self,
        last_two_times: (i64, i64),
        last_two_slot: (u64, u64),
        last_two_prices: (i64, i64),
        _last_two_confs: (u64, u64),
    ) -> Result<(), OracleError> {
        let epoch_0 = self.time_to_epoch(last_two_times.0)?;
        let epoch_1 = self.time_to_epoch(last_two_times.1)?;

        self.current_entry_weighted_price_accumulator +=
            i128::from(last_two_slot.1 - last_two_slot.0)
                * i128::from(last_two_prices.1 + last_two_prices.0)
                / 2;
        self.current_entry_slot_accumulator += last_two_slot.1 - last_two_slot.0;
        if (last_two_slot.1 - last_two_slot.0) > self.threshold {
            self.current_entry_status = Status::Invalid;
        }

        for i in epoch_0 + 1..epoch_1 {
            self.initialize_epoch(i);
            self.current_entry_weighted_price_accumulator +=
                i128::from(last_two_slot.1 - last_two_slot.0)
                    * (i128::from(last_two_prices.1) + i128::from(last_two_prices.0))
                    / 2;
            self.current_entry_slot_accumulator += last_two_slot.1 - last_two_slot.0;
            if (last_two_slot.1 - last_two_slot.0) > self.threshold {
                self.current_entry_status = Status::Invalid;
            }
        }
        Ok(())
    }

    pub fn initialize_epoch(&mut self, epoch: usize) {
        let index = epoch % NUM_ENTRIES;
        let prev_index = (epoch - 1) % NUM_ENTRIES;
        if self.current_entry_status == Status::Valid {
            self.running_valid_entry_counter[index] =
                self.running_valid_entry_counter[prev_index] + 1;
        }
        self.running_entry_prices[epoch] = self.running_entry_prices[prev_index]
            + self.current_entry_weighted_price_accumulator
                / i128::from(self.current_entry_slot_accumulator);
        self.current_entry_slot_accumulator = 0;
        self.current_entry_weighted_price_accumulator = 0;
        self.current_entry_status = Status::Valid;
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
