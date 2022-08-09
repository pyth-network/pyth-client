use std::borrow::BorrowMut;
use std::cell::RefMut;
use std::mem::{
    size_of,
    size_of_val,
};

use bytemuck::{
    bytes_of,
    bytes_of_mut,
};
use solana_program::account_info::AccountInfo;
use solana_program::entrypoint::SUCCESS;
use solana_program::program_error::ProgramError;
use solana_program::program_memory::{
    sol_memcpy,
    sol_memset,
};
use solana_program::pubkey::Pubkey;
use solana_program::rent::Rent;

use crate::c_oracle_header::{
    cmd_add_price_t,
    cmd_add_publisher_t,
    cmd_del_publisher_t,
    cmd_hdr_t,
    cmd_upd_product_t,
    pc_acc,
    pc_map_table_t,
    pc_price_comp,
    pc_price_t,
    pc_prod_t,
    pc_pub_key_t,
    PythAccount,
    PC_COMP_SIZE,
    PC_MAGIC,
    PC_MAP_TABLE_SIZE,
    PC_MAX_NUM_DECIMALS,
    PC_PROD_ACC_SIZE,
    PC_PTYPE_UNKNOWN,
};
use crate::deserialize::{
    load,
    load_account_as,
    load_account_as_mut,
};
use crate::error::OracleResult;
use crate::utils::pyth_assert;
use crate::OracleError;

use super::c_entrypoint_wrapper;

///Calls the c oracle update_price, and updates the Time Machine if needed
pub fn update_price(
    _program_id: &Pubkey,
    _accounts: &[AccountInfo],
    _instruction_data: &[u8],
    input: *mut u8,
) -> OracleResult {
    //For now, we did not change the behavior of this. this is just to show the proposed structure
    // of the program
    c_entrypoint_wrapper(input)
}
/// has version number/ account type dependant logic to make sure the given account is compatible
/// with the current version
/// updates the version number for all accounts, and resizes price accounts
pub fn update_version(
    _program_id: &Pubkey,
    _accounts: &[AccountInfo],
    _instruction_data: &[u8],
) -> OracleResult {
    panic!("Need to merge fix to pythd in order to implement this");
    // Ok(SUCCESS)
}

/// initialize the first mapping account in a new linked-list of mapping accounts
/// accounts[0] funding account           [signer writable]
/// accounts[1] new mapping account       [signer writable]
pub fn init_mapping(
    program_id: &Pubkey,
    accounts: &[AccountInfo],
    instruction_data: &[u8],
) -> OracleResult {
    let [funding_account, fresh_mapping_account] = match accounts {
        [x, y] => Ok([x, y]),
        _ => Err(ProgramError::InvalidArgument),
    }?;

    check_valid_funding_account(funding_account)?;
    check_valid_signable_account(
        program_id,
        fresh_mapping_account,
        size_of::<pc_map_table_t>(),
    )?;
    check_valid_fresh_account(fresh_mapping_account)?;

    // Initialize by setting to zero again (just in case) and populating the account header
    let hdr = load::<cmd_hdr_t>(instruction_data)?;
    initialize_checked::<pc_map_table_t>(fresh_mapping_account, hdr.ver_)?;

    Ok(SUCCESS)
}

pub fn add_mapping(
    program_id: &Pubkey,
    accounts: &[AccountInfo],
    instruction_data: &[u8],
) -> OracleResult {
    let [funding_account, cur_mapping, next_mapping] = match accounts {
        [x, y, z] => Ok([x, y, z]),
        _ => Err(ProgramError::InvalidArgument),
    }?;

    check_valid_funding_account(funding_account)?;
    check_valid_signable_account(program_id, cur_mapping, size_of::<pc_map_table_t>())?;
    check_valid_signable_account(program_id, next_mapping, size_of::<pc_map_table_t>())?;
    check_valid_fresh_account(next_mapping)?;

    let hdr = load::<cmd_hdr_t>(instruction_data)?;
    let mut cur_mapping = load_checked::<pc_map_table_t>(cur_mapping, hdr.ver_)?;
    pyth_assert(
        cur_mapping.num_ == PC_MAP_TABLE_SIZE && pubkey_is_zero(&cur_mapping.next_),
        ProgramError::InvalidArgument,
    )?;

    initialize_checked::<pc_map_table_t>(next_mapping, hdr.ver_)?;
    pubkey_assign(&mut cur_mapping.next_, &next_mapping.key.to_bytes());

    Ok(SUCCESS)
}

/// add a price account to a product account
/// accounts[0] funding account                                   [signer writable]
/// accounts[1] product account to add the price account to       [signer writable]
/// accounts[2] newly created price account                       [signer writable]
pub fn add_price(
    program_id: &Pubkey,
    accounts: &[AccountInfo],
    instruction_data: &[u8],
) -> OracleResult {
    let cmd_args = load::<cmd_add_price_t>(instruction_data)?;

    if cmd_args.expo_ > PC_MAX_NUM_DECIMALS as i32
        || cmd_args.expo_ < -(PC_MAX_NUM_DECIMALS as i32)
        || cmd_args.ptype_ == PC_PTYPE_UNKNOWN
    {
        return Err(ProgramError::InvalidArgument);
    }

    let [funding_account, product_account, price_account] = match accounts {
        [x, y, z] => Ok([x, y, z]),
        _ => Err(ProgramError::InvalidArgument),
    }?;

    check_valid_funding_account(funding_account)?;
    check_valid_signable_account(program_id, product_account, PC_PROD_ACC_SIZE as usize)?;
    check_valid_signable_account(program_id, price_account, size_of::<pc_price_t>())?;
    check_valid_fresh_account(price_account)?;

    let mut product_data = load_checked::<pc_prod_t>(product_account, cmd_args.ver_)?;

    let mut price_data = initialize_checked::<pc_price_t>(price_account, cmd_args.ver_)?;
    price_data.expo_ = cmd_args.expo_;
    price_data.ptype_ = cmd_args.ptype_;
    pubkey_assign(&mut price_data.prod_, &product_account.key.to_bytes());
    pubkey_assign(&mut price_data.next_, bytes_of(&product_data.px_acc_));
    pubkey_assign(&mut product_data.px_acc_, &price_account.key.to_bytes());

    Ok(SUCCESS)
}

/// add a publisher to a price account
/// accounts[0] funding account                                   [signer writable]
/// accounts[1] price account to add the publisher to             [signer writable]
pub fn add_publisher(
    program_id: &Pubkey,
    accounts: &[AccountInfo],
    instruction_data: &[u8],
) -> OracleResult {
    let cmd_args = load::<cmd_add_publisher_t>(instruction_data)?;

    pyth_assert(
        instruction_data.len() == size_of::<cmd_add_publisher_t>()
            && !pubkey_is_zero(&cmd_args.pub_),
        ProgramError::InvalidArgument,
    )?;

    let [funding_account, price_account] = match accounts {
        [x, y] => Ok([x, y]),
        _ => Err(ProgramError::InvalidArgument),
    }?;

    check_valid_funding_account(funding_account)?;
    check_valid_signable_account(program_id, price_account, size_of::<pc_price_t>())?;

    let mut price_data = load_checked::<pc_price_t>(price_account, cmd_args.ver_)?;

    if price_data.num_ >= PC_COMP_SIZE {
        return Err(ProgramError::InvalidArgument);
    }

    for i in 0..(price_data.num_ as usize) {
        if pubkey_equal(&cmd_args.pub_, bytes_of(&price_data.comp_[i].pub_)) {
            return Err(ProgramError::InvalidArgument);
        }
    }

    let current_index: usize = try_convert(price_data.num_)?;
    sol_memset(
        bytes_of_mut(&mut price_data.comp_[current_index]),
        0,
        size_of::<pc_price_comp>(),
    );
    pubkey_assign(
        &mut price_data.comp_[current_index].pub_,
        bytes_of(&cmd_args.pub_),
    );
    price_data.num_ += 1;
    price_data.size_ =
        try_convert::<_, u32>(size_of::<pc_price_t>() - size_of_val(&price_data.comp_))?
            + price_data.num_ * try_convert::<_, u32>(size_of::<pc_price_comp>())?;
    Ok(SUCCESS)
}

/// add a publisher to a price account
/// accounts[0] funding account                                   [signer writable]
/// accounts[1] price account to delete the publisher from        [signer writable]
pub fn del_publisher(
    program_id: &Pubkey,
    accounts: &[AccountInfo],
    instruction_data: &[u8],
) -> OracleResult {
    let cmd_args = load::<cmd_del_publisher_t>(instruction_data)?;

    pyth_assert(
        instruction_data.len() == size_of::<cmd_del_publisher_t>()
            && !pubkey_is_zero(&cmd_args.pub_),
        ProgramError::InvalidArgument,
    )?;

    let [funding_account, price_account] = match accounts {
        [x, y] => Ok([x, y]),
        _ => Err(ProgramError::InvalidArgument),
    }?;

    check_valid_funding_account(funding_account)?;
    check_valid_signable_account(program_id, price_account, size_of::<pc_price_t>())?;

    let mut price_data = load_checked::<pc_price_t>(price_account, cmd_args.ver_)?;

    for i in 0..(price_data.num_ as usize) {
        if pubkey_equal(&cmd_args.pub_, bytes_of(&price_data.comp_[i].pub_)) {
            for j in i + 1..(price_data.num_ as usize) {
                price_data.comp_[j - 1] = price_data.comp_[j];
            }
            price_data.num_ -= 1;
            let current_index: usize = try_convert(price_data.num_)?;
            sol_memset(
                bytes_of_mut(&mut price_data.comp_[current_index]),
                0,
                size_of::<pc_price_t>(),
            );
            price_data.size_ =
                try_convert::<_, u32>(size_of::<pc_price_t>() - size_of_val(&price_data.comp_))?
                    + price_data.num_ * try_convert::<_, u32>(size_of::<pc_price_comp>())?;
            return Ok(SUCCESS);
        }
    }
    Err(ProgramError::InvalidArgument)
}

pub fn add_product(
    program_id: &Pubkey,
    accounts: &[AccountInfo],
    instruction_data: &[u8],
) -> OracleResult {
    let [funding_account, tail_mapping_account, new_product_account] = match accounts {
        [x, y, z] => Ok([x, y, z]),
        _ => Err(ProgramError::InvalidArgument),
    }?;

    check_valid_funding_account(funding_account)?;
    check_valid_signable_account(
        program_id,
        tail_mapping_account,
        size_of::<pc_map_table_t>(),
    )?;
    check_valid_signable_account(program_id, new_product_account, PC_PROD_ACC_SIZE as usize)?;
    check_valid_fresh_account(new_product_account)?;

    let hdr = load::<cmd_hdr_t>(instruction_data)?;
    let mut mapping_data = load_checked::<pc_map_table_t>(tail_mapping_account, hdr.ver_)?;
    // The mapping account must have free space to add the product account
    pyth_assert(
        mapping_data.num_ < PC_MAP_TABLE_SIZE,
        ProgramError::InvalidArgument,
    )?;

    initialize_checked::<pc_prod_t>(new_product_account, hdr.ver_)?;

    let current_index: usize = try_convert(mapping_data.num_)?;
    pubkey_assign(
        &mut mapping_data.prod_[current_index],
        bytes_of(&new_product_account.key.to_bytes()),
    );
    mapping_data.num_ += 1;
    mapping_data.size_ =
        try_convert::<_, u32>(size_of::<pc_map_table_t>() - size_of_val(&mapping_data.prod_))?
            + mapping_data.num_ * try_convert::<_, u32>(size_of::<pc_pub_key_t>())?;

    Ok(SUCCESS)
}

/// Update the metadata associated with a product, overwriting any existing metadata.
/// The metadata is provided as a list of key-value pairs at the end of the `instruction_data`.
pub fn upd_product(
    program_id: &Pubkey,
    accounts: &[AccountInfo],
    instruction_data: &[u8],
) -> OracleResult {
    let [funding_account, product_account] = match accounts {
        [x, y] => Ok([x, y]),
        _ => Err(ProgramError::InvalidArgument),
    }?;

    check_valid_funding_account(funding_account)?;
    check_valid_signable_account(program_id, product_account, try_convert(PC_PROD_ACC_SIZE)?)?;

    let hdr = load::<cmd_hdr_t>(instruction_data)?;
    {
        // Validate that product_account contains the appropriate account header
        let mut _product_data = load_checked::<pc_prod_t>(product_account, hdr.ver_)?;
    }

    pyth_assert(
        instruction_data.len() >= size_of::<cmd_upd_product_t>(),
        ProgramError::InvalidInstructionData,
    )?;
    let new_data_len = instruction_data.len() - size_of::<cmd_upd_product_t>();
    let max_data_len = try_convert::<_, usize>(PC_PROD_ACC_SIZE)? - size_of::<pc_prod_t>();
    pyth_assert(new_data_len <= max_data_len, ProgramError::InvalidArgument)?;

    let new_data = &instruction_data[size_of::<cmd_upd_product_t>()..instruction_data.len()];
    let mut idx = 0;
    // new_data must be a list of key-value pairs, both of which are instances of pc_str_t.
    // Try reading the key-value pairs to validate that new_data is properly formatted.
    while idx < new_data.len() {
        let key = read_pc_str_t(&new_data[idx..])?;
        idx += key.len();
        let value = read_pc_str_t(&new_data[idx..])?;
        idx += value.len();
    }

    // This assertion shouldn't ever fail, but be defensive.
    pyth_assert(idx == new_data.len(), ProgramError::InvalidArgument)?;

    {
        let mut data = product_account.try_borrow_mut_data()?;
        // Note that this memcpy doesn't necessarily overwrite all existing data in the account.
        // This case is handled by updating the .size_ field below.
        sol_memcpy(
            &mut data[size_of::<pc_prod_t>()..],
            new_data,
            new_data.len(),
        );
    }

    let mut product_data = load_checked::<pc_prod_t>(product_account, hdr.ver_)?;
    product_data.size_ = try_convert(size_of::<pc_prod_t>() + new_data.len())?;

    Ok(SUCCESS)
}

fn valid_funding_account(account: &AccountInfo) -> bool {
    account.is_signer && account.is_writable
}

fn check_valid_funding_account(account: &AccountInfo) -> Result<(), ProgramError> {
    pyth_assert(
        valid_funding_account(account),
        ProgramError::InvalidArgument,
    )
}

fn valid_signable_account(program_id: &Pubkey, account: &AccountInfo, minimum_size: usize) -> bool {
    account.is_signer
        && account.is_writable
        && account.owner == program_id
        && account.data_len() >= minimum_size
        && Rent::default().is_exempt(account.lamports(), account.data_len())
}

fn check_valid_signable_account(
    program_id: &Pubkey,
    account: &AccountInfo,
    minimum_size: usize,
) -> Result<(), ProgramError> {
    pyth_assert(
        valid_signable_account(program_id, account, minimum_size),
        ProgramError::InvalidArgument,
    )
}

/// Returns `true` if the `account` is fresh, i.e., its data can be overwritten.
/// Use this check to prevent accidentally overwriting accounts whose data is already populated.
fn valid_fresh_account(account: &AccountInfo) -> bool {
    let pyth_acc = load_account_as::<pc_acc>(account);
    match pyth_acc {
        Ok(pyth_acc) => pyth_acc.magic_ == 0 && pyth_acc.ver_ == 0,
        Err(_) => false,
    }
}

fn check_valid_fresh_account(account: &AccountInfo) -> Result<(), ProgramError> {
    pyth_assert(valid_fresh_account(account), ProgramError::InvalidArgument)
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

pub fn load_checked<'a, T: PythAccount>(
    account: &'a AccountInfo,
    version: u32,
) -> Result<RefMut<'a, T>, ProgramError> {
    {
        let account_header = load_account_as::<pc_acc>(account)?;
        pyth_assert(
            account_header.magic_ == PC_MAGIC
                && account_header.ver_ == version
                && account_header.type_ == T::ACCOUNT_TYPE,
            ProgramError::InvalidArgument,
        )?;
    }

    load_account_as_mut::<T>(account)
}

pub fn initialize_checked<'a, T: PythAccount>(
    account: &'a AccountInfo,
    version: u32,
) -> Result<RefMut<'a, T>, ProgramError> {
    clear_account(account)?;

    {
        let mut account_header = load_account_as_mut::<pc_acc>(account)?;
        account_header.magic_ = PC_MAGIC;
        account_header.ver_ = version;
        account_header.type_ = T::ACCOUNT_TYPE;
        account_header.size_ = T::INITIAL_SIZE;
    }

    load_account_as_mut::<T>(account)
}

// Assign pubkey bytes from source to target, fails if source is not 32 bytes
pub fn pubkey_assign(target: &mut pc_pub_key_t, source: &[u8]) {
    unsafe { target.k1_.copy_from_slice(source) }
}

pub fn pubkey_is_zero(key: &pc_pub_key_t) -> bool {
    return unsafe { key.k8_.iter().all(|x| *x == 0) };
}

pub fn pubkey_equal(target: &pc_pub_key_t, source: &[u8]) -> bool {
    unsafe { target.k1_ == *source }
}

/// Convert `x: T` into a `U`, returning the appropriate `OracleError` if the conversion fails.
pub fn try_convert<T, U: TryFrom<T>>(x: T) -> Result<U, OracleError> {
    // Note: the error here assumes we're only applying this function to integers right now.
    U::try_from(x).map_err(|_| OracleError::IntegerCastingError)
}

/// Read a `pc_str_t` from the beginning of `source`. Returns a slice of `source` containing
/// the bytes of the `pc_str_t`.
pub fn read_pc_str_t(source: &[u8]) -> Result<&[u8], ProgramError> {
    if source.is_empty() {
        Err(ProgramError::InvalidArgument)
    } else {
        let tag_len: usize = try_convert(source[0])?;
        if tag_len + 1 > source.len() {
            Err(ProgramError::InvalidArgument)
        } else {
            Ok(&source[..(1 + tag_len)])
        }
    }
}
