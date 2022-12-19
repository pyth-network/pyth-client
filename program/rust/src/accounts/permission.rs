use {
    super::{
        AccountHeader,
        PythAccount,
    },
    crate::{
        c_oracle_header::PC_ACCTYPE_PERMISSIONS,
        instruction::OracleCommand,
    },
    bytemuck::{
        Pod,
        Zeroable,
    },
    solana_program::pubkey::Pubkey,
    std::mem::size_of,
};

pub const PERMISSIONS_SEED: &str = "permissions";

/// This account stores the pubkeys that can execute administrative instructions in the Pyth
/// program. Only the upgrade authority of the program can update these permissions.
#[repr(C)]
#[derive(Copy, Clone, Pod, Zeroable)]
pub struct PermissionAccount {
    /// pyth account header
    pub header:                  AccountHeader,
    /// An authority that can do any administrative task
    pub master_authority:        Pubkey,
    /// An authority that can  :
    /// - Add mapping accounts
    /// - Add price accounts
    /// - Add product accounts
    /// - Delete price accounts
    /// - Delete product accounts
    /// - Update product accounts
    pub data_curation_authority: Pubkey,
    /// An authority that can  :
    /// - Add publishers
    /// - Delete publishers
    /// - Set minimum number of publishers
    pub security_authority:      Pubkey,
}

impl PermissionAccount {
    pub fn is_authorized(&self, key: &Pubkey, command: OracleCommand) -> bool {
        #[allow(clippy::match_like_matches_macro)]
        match (*key, command) {
            (pubkey, _) if pubkey == self.master_authority => true,
            (pubkey, OracleCommand::SetMinPub) if pubkey == self.security_authority => true,
            _ => false,
        }
    }
}

impl PythAccount for PermissionAccount {
    const ACCOUNT_TYPE: u32 = PC_ACCTYPE_PERMISSIONS;
    const INITIAL_SIZE: u32 = size_of::<PermissionAccount>() as u32;
}
