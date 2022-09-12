pub trait Authority {
    pub fn can_do(command : OracleCommand) -> bool;
}

impl Authority for MasterAuthority {

}
