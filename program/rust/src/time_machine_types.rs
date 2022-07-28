use super::c_oracle_header::TIME_MACHINE_STRUCT_SIZE;
use ::std::mem::size_of;
use borsh::{BorshDeserialize, BorshSerialize};
#[derive(Debug, Clone, BorshSerialize, BorshDeserialize)]
#[repr(C)]
/// this wraps multiple SMA and tick trackers, and includes all the state
/// used by the time machine
pub struct TimeMachineWrapper {
    //Place holder with the size of the fields I am planning to add
    place_holder: [u8; 1864],
}

impl Default for TimeMachineWrapper {
    fn default() -> TimeMachineWrapper {
        TimeMachineWrapper {
            place_holder: [0; 1864],
        }
    }
}

impl TimeMachineWrapper{
    /// Add a price to the trackers
    /// automatically calls the method below if prev_time=0
    fn add_price(&mut self, current_time: u64, prev_time: u64, current_price: u64, prev_price: u64,  current_conf: u64, prev_conf: u64) {
        panic!("implement me!");
    }
    /// Add a price to the tracker when the tracker is first initialized or
    /// price account is new
    fn first_add_price(&mut self, current_time: u64, current_price: u64,  current_conf: u64) {
        panic!("implement me!");
    }
    /// gets the given entry from the tracker with the
    fn get_entry(&self, current_time: u64, entry_time: u64,granuality: u64, is_twap: bool) -> (u64, u64, u8) {
        panic!("implement me!");
        (0, 0, 0)
    }
}

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
