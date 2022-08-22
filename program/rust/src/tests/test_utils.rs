use crate::c_oracle_header::{
    PythAccount,
    PC_VERSION,
};
use crate::instruction::{
    CommandHeader,
    OracleCommand,
};
use num_traits::ToPrimitive;
use solana_program::account_info::AccountInfo;
use solana_program::clock::Epoch;
use solana_program::native_token::LAMPORTS_PER_SOL;
use solana_program::pubkey::Pubkey;
use solana_program::rent::Rent;
use solana_program::sysvar::{
    Sysvar,
    SysvarId,
};
use solana_program::{
    clock,
    system_program,
    sysvar,
};
const UPPER_BOUND_OF_ALL_ACCOUNT_SIZES: usize = 20536;

/// The goal of this struct is to easily instantiate fresh solana accounts
/// for the Pyth program to use in tests.
/// The reason why we can't just create an `AccountInfo` object
/// is that we need to give ownership of `key`, `owner` and `data` to the outside scope
/// otherwise AccountInfo will become a dangling pointer.
/// After instantiating the setup `AccountSetup` with `new` (that line will transfer the fields to
/// the outer scope),  `to_account_info` gives the user an `AccountInfo` pointing to the fields of
/// the AccountSetup.
pub struct AccountSetup {
    key:     Pubkey,
    owner:   Pubkey,
    balance: u64,
    size:    usize,
    data:    [u8; UPPER_BOUND_OF_ALL_ACCOUNT_SIZES],
}

impl AccountSetup {
    pub fn new<T: PythAccount>(owner: &Pubkey) -> Self {
        let key = Pubkey::new_unique();
        let owner = owner.clone();
        let balance = Rent::minimum_balance(&Rent::default(), T::minimum_size());
        let size = T::minimum_size();
        let data = [0; UPPER_BOUND_OF_ALL_ACCOUNT_SIZES];
        return AccountSetup {
            key,
            owner,
            balance,
            size,
            data,
        };
    }

    pub fn new_funding() -> Self {
        let key = Pubkey::new_unique();
        let owner = system_program::id();
        let balance = LAMPORTS_PER_SOL;
        let size = 0;
        let data = [0; UPPER_BOUND_OF_ALL_ACCOUNT_SIZES];
        return AccountSetup {
            key,
            owner,
            balance,
            size,
            data,
        };
    }

    pub fn new_clock() -> Self {
        let key = clock::Clock::id();
        let owner = sysvar::id();
        let balance = Rent::minimum_balance(&Rent::default(), clock::Clock::size_of());
        let size = clock::Clock::size_of();
        let data = [0u8; UPPER_BOUND_OF_ALL_ACCOUNT_SIZES];
        return AccountSetup {
            key,
            owner,
            balance,
            size,
            data,
        };
    }

    pub fn to_account_info(&mut self) -> AccountInfo {
        return AccountInfo::new(
            &self.key,
            true,
            true,
            &mut self.balance,
            &mut self.data[..self.size],
            &self.owner,
            false,
            Epoch::default(),
        );
    }
}

pub fn update_clock_slot(clock_account: &mut AccountInfo, slot: u64) {
    let mut clock_data = clock::Clock::from_account_info(clock_account).unwrap();
    clock_data.slot = slot;
    clock_data.to_account_info(clock_account);
}

impl Into<CommandHeader> for OracleCommand {
    fn into(self) -> CommandHeader {
        return CommandHeader {
            version: PC_VERSION,
            command: self.to_i32().unwrap(), // This can never fail
        };
    }
}
