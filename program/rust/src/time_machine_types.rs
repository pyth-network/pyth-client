use crate::c_oracle_header::{
    pc_price_t,
    EXTRA_PUBLISHER_SPACE,
};
use crate::error::OracleError;
use crate::utils::try_convert;
use bytemuck::{
    Pod,
    Zeroable,
};
use solana_program::msg;

pub trait Tracker {
    /// add the first price to a zero initialized tracker
    fn add_first_price(
        &mut self,
        current_time: u64,
        current_price: i64,
        current_conf: u64,
    ) -> Result<(), OracleError>;

    ///add a new price to a tracker
    fn add_price(
        &mut self,
        prev_time: u64,
        prev_price: i64,
        prev_conf: u64,
        current_time: u64,
        current_price: i64,
        current_conf: u64,
    ) -> Result<(), OracleError>;
}

#[derive(Debug, Clone, Copy)]
#[repr(C)]
/// this wraps multiple SMA and tick trackers, and includes all the state
/// used by the time machine
pub struct TimeMachineWrapper {
    //Place holder with the size of the fields I am planning to add
    place_holder: [u8; 1864],
}

impl Tracker for TimeMachineWrapper {
    fn add_first_price(
        &mut self,
        _current_time: u64,
        _current_price: i64,
        _current_conf: u64,
    ) -> Result<(), OracleError> {
        msg!("implement me");
        Ok(())
    }
    fn add_price(
        &mut self,
        _prev_time: u64,
        _prev_price: i64,
        _prev_conf: u64,
        _current_time: u64,
        _current_price: i64,
        _current_conf: u64,
    ) -> Result<(), OracleError> {
        msg!("implement me");
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
    pub fn add_first_price(&mut self) -> Result<(), OracleError> {
        self.time_machine.add_first_price(
            try_convert(self.price_data.timestamp_)?,
            self.price_data.agg_.price_,
            self.price_data.agg_.conf_,
        )
    }

    pub fn add_price(&mut self) -> Result<(), OracleError> {
        self.time_machine.add_price(
            try_convert(self.price_data.prev_timestamp_)?,
            self.price_data.prev_price_,
            self.price_data.prev_conf_,
            try_convert(self.price_data.timestamp_)?,
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
