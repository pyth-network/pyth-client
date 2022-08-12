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
use solana_program::msg;
use solana_program::program_error::ProgramError;

//To make it easy to change the types to allow for more usefull
//running sums if needed
use {
    i64 as SignedTrackerRunningSum,
    u64 as UnsignedTrackerRunningSum,
};


pub trait Tracker<const GRANUALITY: i64, const NUM_ENTRIES: usize, const THRESHOLD: i64> {
    ///initializes a zero initialized tracker
    fn initialize(&mut self) -> Result<(), ProgramError>;

    ///add a new price to the tracker
    fn add_price(
        &mut self,
        prev_time: i64,
        prev_price: i64,
        prev_conf: u64,
        current_price: i64,
        current_time: i64,
        current_conf: u64,
    ) -> Result<(), ProgramError>;
    fn get_num_skipped_entries_capped(
        prev_time: i64,
        current_time: i64,
    ) -> Result<usize, OracleError> {
        let time_since_begining_of_current_entry =
            (prev_time % GRANUALITY) + current_time - prev_time;
        let mut num_skiped_entries: usize =
            try_convert(time_since_begining_of_current_entry / GRANUALITY)?;
        if num_skiped_entries >= NUM_ENTRIES {
            num_skiped_entries %= NUM_ENTRIES;
            num_skiped_entries += NUM_ENTRIES;
        }
        Ok(num_skiped_entries)
    }
    fn get_next_entr(current_entry: usize) -> usize {
        (current_entry + 1) % NUM_ENTRIES
    }
}

#[derive(Copy, Clone)]
#[repr(C)]
/// Represents an SMA Tracker that has NUM_ENTRIES entries
/// each tracking time weighted for GRANUALITY seconds periods.
///The prices are assumed to be provided under some fixed point representation, and the computation
/// gurantees accuracy up to the last decimal digit in the fixed point representation.
pub struct SmaTracker<const GRANUALITY: i64, const NUM_ENTRIES: usize, const THRESHOLD: i64> {
    valid_entry_counter:        u64, // is incremented everytime we add a valid entry
    max_update_time:            i64, // maximum time between two updates in the current entry.
    running_price:              [SignedTrackerRunningSum; NUM_ENTRIES], /* price running time
                                      * weighted sums */
    running_confidence:         [UnsignedTrackerRunningSum; NUM_ENTRIES], /* confidence running
                                                                           * time
                                                                           * weighted sums */
    current_entry:              usize, //the current entry
    max_observed_entry_abs_val: u64,   /* used to ensure that there are no overflows when
                                        * looking at running sum diffs */
    entry_validity:             [u8; NUM_ENTRIES], //entry validity
}


#[derive(Copy, Clone)]
#[repr(C)]
/// Represents an Tick Tracker that has NUM_ENTRIES entries
/// each tracking the last tick before every GRANUALITY seconds.
pub struct TickTracker<const GRANUALITY: i64, const NUM_ENTRIES: usize, const THRESHOLD: i64> {
    running_price:      [SignedTrackerRunningSum; NUM_ENTRIES], /* price running time
                                                                 * weighted sums */
    running_confidence: [UnsignedTrackerRunningSum; NUM_ENTRIES], /* confidence running
                                                                   * time
                                                                   * weighted sums */
    current_entry:      usize,             //the current entry
    entry_validity:     [u8; NUM_ENTRIES], //entry validity
}


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
    pub price_data:            pc_price_t,
    //space for more publishers
    pub extra_publisher_space: [u8; EXTRA_PUBLISHER_SPACE as usize],
    //TimeMachine
    pub time_machine:          TimeMachineWrapper,
}
impl PriceAccountWrapper {
    pub fn initialize_time_machine(&mut self) -> Result<(), OracleError> {
        msg!("implement me");
        Ok(())
    }

    pub fn add_price_to_time_machine(&mut self) -> Result<(), OracleError> {
        msg!("implement me");
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
