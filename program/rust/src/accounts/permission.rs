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
    solana_program::{
        account_info::AccountInfo,
        program_error::ProgramError,
        pubkey::Pubkey,
    },
    std::{
        cell::RefMut,
        mem::size_of,
    },
};

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
    pub const MIN_SIZE_WITH_LAST_FEED_INDEX: usize =
        size_of::<PermissionAccount>() + size_of::<u32>();

    pub fn is_authorized(&self, key: &Pubkey, command: OracleCommand) -> bool {
        #[allow(clippy::match_like_matches_macro)]
        match (*key, command) {
            (pubkey, _) if pubkey == self.master_authority => true,
            (pubkey, OracleCommand::ResizePriceAccount) if pubkey == self.security_authority => {
                true
            } // Allow for an admin key to resize the price account
            _ => false,
        }
    }

    pub fn load_last_feed_index_mut<'a>(
        account: &'a AccountInfo,
    ) -> Result<RefMut<'a, u32>, ProgramError> {
        let start = size_of::<PermissionAccount>();
        let end = start + size_of::<u32>();
        assert_eq!(Self::MIN_SIZE_WITH_LAST_FEED_INDEX, end);
        if account.data_len() < end {
            return Err(ProgramError::AccountDataTooSmall);
        }
        Ok(RefMut::map(account.try_borrow_mut_data()?, |data| {
            bytemuck::from_bytes_mut(&mut data[start..end])
        }))
    }
}

impl PythAccount for PermissionAccount {
    const ACCOUNT_TYPE: u32 = PC_ACCTYPE_PERMISSIONS;
    // TODO: change?
    // TODO: add feed_index when creating account
    const INITIAL_SIZE: u32 = size_of::<PermissionAccount>() as u32;
}
