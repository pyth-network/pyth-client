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
