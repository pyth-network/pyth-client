use {
    crate::{
        accounts::{
            PermissionAccount,
            PythAccount,
            PERMISSIONS_SEED,
        },
        c_oracle_header::PC_VERSION,
        error::OracleError,
        instruction::{
            CommandHeader,
            OracleCommand,
        },
    },
    num_traits::ToPrimitive,
    solana_program::{
        account_info::AccountInfo,
        clock::{
            self,
            Epoch,
        },
        instruction::InstructionError,
        native_token::LAMPORTS_PER_SOL,
        program_error::ProgramError,
        pubkey::Pubkey,
        rent::Rent,
        system_program,
        sysvar::{
            self,
            Sysvar,
            SysvarId,
        },
    },
    solana_sdk::transaction::TransactionError,
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
#[repr(align(16))] // On Apple systems this is needed to support u128 in the struct
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
        let owner = *owner;
        let balance = Rent::minimum_balance(&Rent::default(), T::MINIMUM_SIZE);
        let size = T::MINIMUM_SIZE;
        let data = [0; UPPER_BOUND_OF_ALL_ACCOUNT_SIZES];
        AccountSetup {
            key,
            owner,
            balance,
            size,
            data,
        }
    }

    pub fn new_funding() -> Self {
        let key = Pubkey::new_unique();
        let owner = system_program::id();
        let balance = LAMPORTS_PER_SOL;
        let size = 0;
        let data = [0; UPPER_BOUND_OF_ALL_ACCOUNT_SIZES];
        AccountSetup {
            key,
            owner,
            balance,
            size,
            data,
        }
    }

    pub fn new_permission(owner: &Pubkey) -> Self {
        let (key, _bump) = Pubkey::find_program_address(&[PERMISSIONS_SEED.as_bytes()], owner);
        let owner = *owner;
        let balance = Rent::minimum_balance(&Rent::default(), PermissionAccount::MINIMUM_SIZE);
        let size = PermissionAccount::MINIMUM_SIZE;
        let data = [0; UPPER_BOUND_OF_ALL_ACCOUNT_SIZES];
        AccountSetup {
            key,
            owner,
            balance,
            size,
            data,
        }
    }

    pub fn new_clock() -> Self {
        let key = clock::Clock::id();
        let owner = sysvar::id();
        let balance = Rent::minimum_balance(&Rent::default(), clock::Clock::size_of());
        let size = clock::Clock::size_of();
        let data = [0u8; UPPER_BOUND_OF_ALL_ACCOUNT_SIZES];
        AccountSetup {
            key,
            owner,
            balance,
            size,
            data,
        }
    }

    pub fn as_account_info(&mut self) -> AccountInfo {
        AccountInfo::new(
            &self.key,
            true,
            true,
            &mut self.balance,
            &mut self.data[..self.size],
            &self.owner,
            false,
            Epoch::default(),
        )
    }
}

pub fn update_clock_slot(clock_account: &mut AccountInfo, slot: u64) {
    let mut clock_data = clock::Clock::from_account_info(clock_account).unwrap();
    clock_data.slot = slot;
    clock_data.to_account_info(clock_account);
}

impl From<OracleCommand> for CommandHeader {
    fn from(val: OracleCommand) -> Self {
        CommandHeader {
            version: PC_VERSION,
            command: val.to_i32().unwrap(), // This can never fail and is only used in tests
        }
    }
}

impl From<OracleError> for TransactionError {
    fn from(error: OracleError) -> Self {
        TransactionError::InstructionError(
            0,
            InstructionError::try_from(u64::from(ProgramError::from(error))).unwrap(),
        )
    }
}
