use crate::c_oracle_header::{
    PriceAccount,
    PythAccount,
};
use crate::error::OracleError;
use bytemuck::{
    Pod,
    Zeroable,
};

const EXTRA_PUBLISHER_SPACE: usize = 1000;

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
    pub extra_publisher_space: [u8; EXTRA_PUBLISHER_SPACE],
    //TimeMachine
    pub time_machine:          TimeMachineWrapper,
}
impl PriceAccountWrapper {
    pub fn initialize_time_machine(&mut self) -> Result<(), OracleError> {
        // TO DO
        Ok(())
    }

    pub fn add_price_to_time_machine(&mut self) -> Result<(), OracleError> {
        // TO DO
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
    const ACCOUNT_TYPE: u32 = PriceAccount::ACCOUNT_TYPE;
    const INITIAL_SIZE: u32 = PriceAccount::INITIAL_SIZE;
}
