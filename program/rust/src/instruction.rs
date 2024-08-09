use {
    crate::{
        c_oracle_header::PC_VERSION,
        deserialize::load,
        error::OracleError,
    },
    bytemuck::{
        Pod,
        Zeroable,
    },
    num_derive::{
        FromPrimitive,
        ToPrimitive,
    },
    num_traits::FromPrimitive,
    solana_program::pubkey::Pubkey,
};

/// WARNING : NEW COMMANDS SHOULD BE ADDED AT THE END OF THE LIST
#[repr(i32)]
#[derive(PartialEq, Eq, FromPrimitive, ToPrimitive)]
pub enum OracleCommand {
    /// Initialize first mapping list account
    // account[0] funding account       [signer writable]
    // account[1] mapping account       [signer writable]
    InitMapping           = 0,
    /// Initialize and add new mapping account
    // account[0] funding account       [signer writable]
    // account[1] tail mapping account  [signer writable]
    // account[2] new mapping account   [signer writable]
    AddMapping            = 1,
    /// Initialize and add new product reference data account
    // account[0] funding account       [signer writable]
    // account[1] mapping account       [signer writable]
    // account[2] new product account   [signer writable]
    AddProduct            = 2,
    /// Update product account
    // account[0] funding account       [signer writable]
    // account[1] product account       [signer writable]
    UpdProduct            = 3,
    /// Add new price account to a product account
    // account[0] funding account       [signer writable]
    // account[1] product account       [signer writable]
    // account[2] new price account     [signer writable]
    AddPrice              = 4,
    /// Add publisher to symbol account
    // account[0] funding account       [signer writable]
    // account[1] price account         [signer writable]
    AddPublisher          = 5,
    /// Delete publisher from symbol account
    // account[0] funding account       [signer writable]
    // account[1] price account         [signer writable]
    DelPublisher          = 6,
    /// Publish component price
    // account[0] funding account       [signer writable]
    // account[1] price account         [writable]
    // account[2] sysvar_clock account  []
    UpdPrice              = 7,
    /// Compute aggregate price
    // account[0] funding account       [signer writable]
    // account[1] price account         [writable]
    // account[2] sysvar_clock account  []
    AggPrice              = 8,
    /// (Re)initialize price account
    // account[0] funding account       [signer writable]
    // account[1] new price account     [signer writable]
    InitPrice             = 9,
    /// deprecated
    InitTest              = 10,
    /// deprecated
    UpdTest               = 11,
    /// Set min publishers
    // account[0] funding account       [signer writable]
    // account[1] price account         [signer writable]
    SetMinPub             = 12,
    /// Publish component price, never returning an error even if the update failed
    // account[0] funding account       [signer writable]
    // account[1] price account         [writable]
    // account[2] sysvar_clock account  []
    UpdPriceNoFailOnError = 13,
    /// Resizes a price account so that it fits the Time Machine
    // account[0] funding account       [signer writable]
    // account[1] price account         [signer writable]
    // account[2] system program        []
    ResizePriceAccount    = 14,
    /// Deletes a price account
    // account[0] funding account       [signer writable]
    // account[1] product account       [signer writable]
    // account[2] price account         [signer writable]
    DelPrice              = 15,
    /// Deletes a product account
    // key[0] funding account       [signer writable]
    // key[1] mapping account       [signer writable]
    // key[2] product account       [signer writable]
    DelProduct            = 16,
    /// Update authorities
    // key[0] upgrade authority         [signer writable]
    // key[1] programdata account       []
    // key[2] permissions account       [writable]
    // key[3] system program            []
    UpdPermissions        = 17,
    /// Set max latency
    // account[0] funding account       [signer writable]
    // account[1] price account         [signer writable]
    SetMaxLatency         = 18,
    /// Init price feed index
    // account[0] funding account        [signer writable]
    // account[1] price account          [writable]
    // account[2] permissions account    [writable]
    // account[3] system program account []
    InitPriceFeedIndex    = 19,
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

#[repr(C)]
#[derive(Zeroable, Pod, Copy, Clone)]
pub struct AddPriceArgs {
    pub header:     CommandHeader,
    pub exponent:   i32,
    pub price_type: u32,
}
pub type InitPriceArgs = AddPriceArgs;

#[repr(C)]
#[derive(Zeroable, Pod, Copy, Clone)]
pub struct AddPublisherArgs {
    pub header:    CommandHeader,
    pub publisher: Pubkey,
}

pub type DelPublisherArgs = AddPublisherArgs;

#[repr(C)]
#[derive(Zeroable, Clone, Copy, Pod)]
pub struct SetMinPubArgs {
    pub header:             CommandHeader,
    pub minimum_publishers: u8,
    pub unused_:            [u8; 3],
}

#[repr(C)]
#[derive(Zeroable, Pod, Copy, Clone)]
pub struct UpdPriceArgs {
    pub header:          CommandHeader,
    pub status:          u32,
    pub unused_:         u32,
    pub price:           i64,
    pub confidence:      u64,
    pub publishing_slot: u64,
}

#[repr(C)]
#[derive(Zeroable, Pod, Copy, Clone)]
pub struct UpdPermissionsArgs {
    pub header:                  CommandHeader,
    pub master_authority:        Pubkey,
    pub data_curation_authority: Pubkey,
    pub security_authority:      Pubkey,
}

#[repr(C)]
#[derive(Zeroable, Clone, Copy, Pod)]
pub struct SetMaxLatencyArgs {
    pub header:      CommandHeader,
    pub max_latency: u8,
    pub unused_:     [u8; 3],
}
