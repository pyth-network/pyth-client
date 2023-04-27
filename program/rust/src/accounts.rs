//! Account types and utilities for working with Pyth accounts.

use {
    crate::{
        c_oracle_header::PC_MAGIC,
        deserialize::load_account_as_mut,
        error::OracleError,
        utils::{
            check_valid_fresh_account,
            get_rent,
            pyth_assert,
            try_convert,
        },
    },
    bytemuck::{
        Pod,
        Zeroable,
    },
    solana_program::{
        account_info::AccountInfo,
        program::invoke_signed,
        program_error::ProgramError,
        program_memory::sol_memset,
        pubkey::Pubkey,
        system_instruction::create_account,
    },
    std::{
        borrow::BorrowMut,
        cell::RefMut,
        mem::size_of,
    },
};

mod mapping;
mod permission;
mod price;
mod product;

#[cfg(test)]
pub use product::{
    account_has_key_values,
    create_pc_str_t,
};
pub use {
    mapping::MappingAccount,
    permission::PermissionAccount,
    price::{
        PriceAccount,
        PriceComponent,
        PriceEma,
        PriceFeedMessage,
        PriceInfo,
    },
    product::{
        read_pc_str_t,
        update_product_metadata,
        ProductAccount,
    },
};

// PDA seeds for accounts.
/// There is a single permissions account under `PERMISSIONS_SEED` that stores which keys
/// are authorized to perform certain adminsitrative actions.
pub const PERMISSIONS_SEED: &str = "permissions";
/// The update price instruction can optionally invoke another program via CPI. The
/// CPI will be signed with the PDA `[UPD_PRICE_WRITE_SEED, invoked_program_public_key]`
/// such that the caller can authenticate its origin.
pub const UPD_PRICE_WRITE_SEED: &str = "upd_price_write";

#[repr(C)]
#[derive(Copy, Clone, Zeroable, Pod)]
pub struct AccountHeader {
    pub magic_number: u32,
    pub version:      u32,
    pub account_type: u32,
    pub size:         u32,
}

/// The PythAccount trait's purpose is to attach constants to the 3 types of accounts that Pyth has
/// (mapping, price, product). This allows less duplicated code, because now we can create generic
/// functions to perform common checks on the accounts and to load and initialize the accounts.
pub trait PythAccount: Pod {
    /// `ACCOUNT_TYPE` is just the account discriminator, it is different for mapping, product and
    /// price
    const ACCOUNT_TYPE: u32;

    /// `INITIAL_SIZE` is the value that the field `size_` will take when the account is first
    /// initialized this one is slightly tricky because for mapping (resp. price) `size_` won't
    /// include the unpopulated entries of `prod_` (resp. `comp_`). At the beginning there are 0
    /// products (resp. 0 components) therefore `INITIAL_SIZE` will be equal to the offset of
    /// `prod_` (resp. `comp_`)  Similarly the product account `INITIAL_SIZE` won't include any
    /// key values.
    const INITIAL_SIZE: u32;

    /// `minimum_size()` is the minimum size that the solana account holding the struct needs to
    /// have. `INITIAL_SIZE` <= `minimum_size()`
    const MINIMUM_SIZE: usize = size_of::<Self>();

    /// Given an `AccountInfo`, verify it is sufficiently large and has the correct discriminator.
    fn initialize<'a>(
        account: &'a AccountInfo,
        version: u32,
    ) -> Result<RefMut<'a, Self>, ProgramError> {
        pyth_assert(
            account.data_len() >= Self::MINIMUM_SIZE,
            OracleError::AccountTooSmall.into(),
        )?;

        check_valid_fresh_account(account)?;
        clear_account(account)?;

        {
            let mut account_header = load_account_as_mut::<AccountHeader>(account)?;
            account_header.magic_number = PC_MAGIC;
            account_header.version = version;
            account_header.account_type = Self::ACCOUNT_TYPE;
            account_header.size = Self::INITIAL_SIZE;
        }
        load_account_as_mut::<Self>(account)
    }

    /// Creates PDA accounts only when needed, and initializes it as one of the Pyth accounts.
    /// This PDA initialization assumes that the account has 0 lamports.
    /// TO DO: Fix this once we can resize the program.
    fn initialize_pda<'a>(
        account: &AccountInfo<'a>,
        funding_account: &AccountInfo<'a>,
        system_program: &AccountInfo<'a>,
        program_id: &Pubkey,
        seeds: &[&[u8]],
        version: u32,
    ) -> Result<(), ProgramError> {
        let target_rent = get_rent()?.minimum_balance(Self::MINIMUM_SIZE);

        if account.data_len() == 0 {
            create(
                funding_account,
                account,
                system_program,
                program_id,
                Self::MINIMUM_SIZE,
                target_rent,
                seeds,
            )?;
            Self::initialize(account, version)?;
        }

        Ok(())
    }
}

fn create<'a>(
    from: &AccountInfo<'a>,
    to: &AccountInfo<'a>,
    system_program: &AccountInfo<'a>,
    owner: &Pubkey,
    space: usize,
    lamports: u64,
    seeds: &[&[u8]],
) -> Result<(), ProgramError> {
    let create_instruction = create_account(from.key, to.key, lamports, try_convert(space)?, owner);
    invoke_signed(
        &create_instruction,
        &[from.clone(), to.clone(), system_program.clone()],
        &[seeds],
    )?;
    Ok(())
}

/// Sets the data of account to all-zero
pub fn clear_account(account: &AccountInfo) -> Result<(), ProgramError> {
    let mut data = account
        .try_borrow_mut_data()
        .map_err(|_| ProgramError::InvalidArgument)?;
    let length = data.len();
    sol_memset(data.borrow_mut(), 0, length);
    Ok(())
}
