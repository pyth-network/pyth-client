use super::c_oracle_header;
#[derive(Debug, Clone)]
#[repr(C)]
/// this wraps all the structs we plan on using
pub struct TimeMachineWrapper{
    //Place holder with the size of the fields I am planning to add
    place_holder: [u8;1864],
}

#[test]
fn c_time_machine_size_is_correct() {
    assert_eq!(
        ::std::mem::size_of::<TimeMachineWrapper>(),
        c_oracle_header::TIME_MACHINE_SIZE.try_into().unwrap(),
        "TIME_MACHINE_SIZE in oracle.h must be changed from {} to {}",
        c_oracle_header::TIME_MACHINE_SIZE,
        ::std::mem::size_of::<TimeMachineWrapper>()
    );
}