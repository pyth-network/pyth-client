use crate::c_oracle_header::PC_VERSION;
use crate::deserialize::load;
use crate::error::OracleError;
use bytemuck::{
    Pod,
    Zeroable,
};
use num_derive::{
    FromPrimitive,
    ToPrimitive,
};
use num_traits::FromPrimitive;

#[repr(i32)]
#[derive(FromPrimitive, ToPrimitive)]
pub enum OracleCommand {
    // initialize first mapping list account
    // account[0] funding account       [signer writable]
    // account[1] mapping account       [signer writable]
    InitMapping,
    // initialize and add new mapping account
    // account[0] funding account       [signer writable]
    // account[1] tail mapping account  [signer writable]
    // account[2] new mapping account   [signer writable]
    AddMapping,
    // initialize and add new product reference data account
    // account[0] funding account       [signer writable]
    // account[1] mapping account       [signer writable]
    // account[2] new product account   [signer writable]
    AddProduct,
    // update product account
    // account[0] funding account       [signer writable]
    // account[1] product account       [signer writable]
    UpdProduct,
    // add new price account to a product account
    // account[0] funding account       [signer writable]
    // account[1] product account       [signer writable]
    // account[2] new price account     [signer writable]
    AddPrice,
    // add publisher to symbol account
    // account[0] funding account       [signer writable]
    // account[1] price account         [signer writable]
    AddPublisher,
    // delete publisher from symbol account
    // account[0] funding account       [signer writable]
    // account[1] price account         [signer writable]
    DelPublisher,
    // publish component price
    // account[0] funding account       [signer writable]
    // account[1] price account         [writable]
    // account[2] sysvar_clock account  []
    UpdPrice,
    // compute aggregate price
    // account[0] funding account       [signer writable]
    // account[1] price account         [writable]
    // account[2] sysvar_clock account  []
    AggPrice,
    // (re)initialize price account
    // account[0] funding account       [signer writable]
    // account[1] new price account     [signer writable]
    InitPrice,
    // deprecated
    InitTest,
    // deprecated
    UpdTest,
    // set min publishers
    // account[0] funding account       [signer writable]
    // account[1] price account         [signer writable]
    SetMinPub,
    // publish component price, never returning an error even if the update failed
    // account[0] funding account       [signer writable]
    // account[1] price account         [writable]
    // account[2] sysvar_clock account  []
    UpdPriceNoFailOnError,
    // resizes a price account so that it fits the Time Machine
    // account[0] funding account       [signer writable]
    // account[1] price account         [signer writable]
    // account[2] system program        []
    ResizePriceAccount,
    // deletes a price account
    // account[0] funding account       [signer writable]
    // account[1] product account       [signer writable]
    // account[2] price account         [signer writable]
    DelPrice,
}

#[repr(C)]
#[derive(Zeroable, Pod, Copy, Clone)]
pub struct CommandHeader {
    pub version: u32,
    pub command: i32,
}

pub fn load_command_header_checked(data: &[u8]) -> Result<OracleCommand, OracleError> {
    let command_header = load::<CommandHeader>(data)?;

    if command_header.version != PC_VERSION {
        return Err(OracleError::InvalidInstructionVersion);
    }
    OracleCommand::from_i32(command_header.command).ok_or(OracleError::UnrecognizedInstruction)
}
