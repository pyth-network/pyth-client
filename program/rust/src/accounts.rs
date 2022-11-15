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
            send_lamports,
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
        system_instruction::{
            allocate,
            assign,
        },
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

pub use {
    mapping::MappingAccount,
    permission::{
        PermissionAccount,
        MASTER_AUTHORITY,
        PERMISSIONS_SEED,
    },
    price::{
        PriceAccount,
        PriceComponent,
        PriceEma,
        PriceInfo,
    },
    product::ProductAccount,
};

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

    // Creates PDA accounts only when needed, and initializes it as one of the Pyth accounts
    fn initialize_pda<'a>(
        account: &AccountInfo<'a>,
        funding_account: &AccountInfo<'a>,
        system_program: &AccountInfo<'a>,
        program_id: &Pubkey,
        seeds: &[&[u8]],
        version: u32,
    ) -> Result<(), ProgramError> {
        let target_rent = get_rent()?.minimum_balance(Self::MINIMUM_SIZE);

        if account.lamports() < target_rent {
            send_lamports(
                funding_account,
                account,
                system_program,
                target_rent - account.lamports(),
            )?;
        }

        if account.data_len() == 0 {
            allocate_data(account, system_program, Self::MINIMUM_SIZE, seeds)?;
            assign_owner(account, program_id, system_program, seeds)?;
            Self::initialize(account, version)?;
        }
        Ok(())
    }
}

/// Given an already empty `AccountInfo`, allocate the data field to the given size. This make no
/// assumptions about owner.
fn allocate_data<'a>(
    account: &AccountInfo<'a>,
    system_program: &AccountInfo<'a>,
    space: usize,
    seeds: &[&[u8]],
) -> Result<(), ProgramError> {
    let allocate_instruction = allocate(account.key, try_convert(space)?);
    invoke_signed(
        &allocate_instruction,
        &[account.clone(), system_program.clone()],
        &[seeds],
    )?;
    Ok(())
}

/// Given a newly created `AccountInfo`, assign the owner to the given program id.
#[allow(dead_code)]
fn assign_owner<'a>(
    account: &AccountInfo<'a>,
    owner: &Pubkey,
    system_program: &AccountInfo<'a>,
    seeds: &[&[u8]],
) -> Result<(), ProgramError> {
    let assign_instruction = assign(account.key, owner);
    invoke_signed(
        &assign_instruction,
        &[account.clone(), system_program.clone()],
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
