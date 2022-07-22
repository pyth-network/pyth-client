use super::c_oracle_header;
#[derive(Debug, Clone)]
#[repr(C)]
/// this wraps multiple SMA and tick trackers, and includes all the state
/// used by the time machine
pub struct TimeMachineWrapper {
    //Place holder with the size of the fields I am planning to add
    place_holder: [u8; 1864],
}

#[test]
///test that the size defined in C matches that
///defined in Rust
fn c_time_machine_size_is_correct() {
    assert_eq!(
        ::std::mem::size_of::<TimeMachineWrapper>(),
        c_oracle_header::TIME_MACHINE_STRUCT_SIZE.try_into().unwrap(),
        "expected TIME_MACHINE_STRUCT_SIZE ({}) in oracle.h to the same as the size of TimeMachineWrapper ({})",
        c_oracle_header::TIME_MACHINE_STRUCT_SIZE,
        ::std::mem::size_of::<TimeMachineWrapper>()
    );
}
