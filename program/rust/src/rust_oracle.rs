use std::borrow::BorrowMut;
use std::cell::RefMut;
use std::mem::{
    size_of,
    size_of_val,
};

use crate::deserialize::{
    load,
    load_account_as,
    load_account_as_mut,
};
use bytemuck::{
    bytes_of,
    bytes_of_mut,
};

use solana_program::account_info::AccountInfo;
use solana_program::entrypoint::SUCCESS;
use solana_program::program_error::ProgramError;
use solana_program::program_memory::sol_memset;
use solana_program::pubkey::Pubkey;
use solana_program::rent::Rent;

use crate::c_oracle_header::{
    cmd_add_price_t,
    cmd_add_publisher_t,
    cmd_hdr_t,
    pc_acc,
    pc_map_table_t,
    pc_price_comp,
    pc_price_t,
    pc_prod_t,
    pc_pub_key_t,
    PC_ACCTYPE_MAPPING,
    PC_ACCTYPE_PRICE,
    PC_ACCTYPE_PRODUCT,
    PC_COMP_SIZE,
    PC_MAGIC,
    PC_MAP_TABLE_SIZE,
    PC_MAX_NUM_DECIMALS,
    PC_PROD_ACC_SIZE,
    PC_PTYPE_UNKNOWN,
};
use crate::error::OracleResult;
use crate::OracleError;

use crate::utils::pyth_assert;

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
    initialize_mapping_account(fresh_mapping_account, hdr.ver_)?;

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
    let mut cur_mapping = load_mapping_account_mut(cur_mapping, hdr.ver_)?;
    pyth_assert(
        cur_mapping.num_ == PC_MAP_TABLE_SIZE
            && unsafe { cur_mapping.next_.k8_.iter().all(|x| *x == 0) },
        ProgramError::InvalidArgument,
    )?;

    initialize_mapping_account(next_mapping, hdr.ver_)?;
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

    let mut product_data = load_product_account_mut(product_account, cmd_args.ver_)?;

    clear_account(price_account)?;

    let mut price_data = load_account_as_mut::<pc_price_t>(price_account)?;
    price_data.magic_ = PC_MAGIC;
    price_data.ver_ = cmd_args.ver_;
    price_data.type_ = PC_ACCTYPE_PRICE;
    price_data.size_ = (size_of::<pc_price_t>() - size_of_val(&price_data.comp_)) as u32;
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

    if instruction_data.len() != size_of::<cmd_add_publisher_t>() || pubkey_is_zero(&cmd_args.pub_)
    {
        return Err(ProgramError::InvalidArgument);
    }

    let [_funding_account, price_account] = match accounts {
        [x, y]
            if valid_funding_account(x)
                && valid_signable_account(program_id, y, size_of::<pc_price_t>()) =>
        {
            Ok([x, y])
        }
        _ => Err(ProgramError::InvalidArgument),
    }?;

    let mut price_data = load_price_account_mut(price_account, cmd_args.ver_)?;

    if price_data.num_ >= PC_COMP_SIZE {
        return Err(ProgramError::InvalidArgument);
    }

    for i in 0..(price_data.num_ as usize) {
        if pubkey_equal(&cmd_args.pub_, &price_data.comp_[i].pub_) {
            return Err(ProgramError::InvalidArgument);
        }
    }

    let next_index = price_data.num_ as usize;
    sol_memset(
        bytes_of_mut(&mut price_data.comp_[next_index]),
        0,
        size_of::<pc_price_comp>(),
    );
    pubkey_assign(
        &mut price_data.comp_[next_index].pub_,
        bytes_of(&cmd_args.pub_),
    );
    price_data.num_ += 1;


    price_data.size_ = (size_of::<pc_price_t>() - size_of_val(&price_data.comp_)
        + (price_data.num_ as usize) * size_of::<pc_price_comp>()) as u32;
    Ok(SUCCESS)
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
    let mut mapping_data = load_mapping_account_mut(tail_mapping_account, hdr.ver_)?;
    // The mapping account must have free space to add the product account
    pyth_assert(
        mapping_data.num_ < PC_MAP_TABLE_SIZE,
        ProgramError::InvalidArgument,
    )?;

    initialize_product_account(new_product_account, hdr.ver_)?;

    let current_index: usize = try_convert(mapping_data.num_)?;
    unsafe {
        mapping_data.prod_[current_index]
            .k1_
            .copy_from_slice(&new_product_account.key.to_bytes())
    }
    mapping_data.num_ += 1;
    mapping_data.size_ =
        try_convert::<_, u32>(size_of::<pc_map_table_t>() - size_of_val(&mapping_data.prod_))?
            + mapping_data.num_ * try_convert::<_, u32>(size_of::<pc_pub_key_t>())?;

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


/// Mutably borrow the data in `account` as a mapping account, validating that the account
/// is properly formatted. Any mutations to the returned value will be reflected in the
/// account data. Use this to read already-initialized accounts.
pub fn load_mapping_account_mut<'a>(
    account: &'a AccountInfo,
    expected_version: u32,
) -> Result<RefMut<'a, pc_map_table_t>, ProgramError> {
    let mapping_data = load_account_as_mut::<pc_map_table_t>(account)?;

    pyth_assert(
        mapping_data.magic_ == PC_MAGIC
            && mapping_data.ver_ == expected_version
            && mapping_data.type_ == PC_ACCTYPE_MAPPING,
        ProgramError::InvalidArgument,
    )?;

    Ok(mapping_data)
}

/// Initialize account as a new mapping account. This function will zero out any existing data in
/// the account.
pub fn initialize_mapping_account(account: &AccountInfo, version: u32) -> Result<(), ProgramError> {
    clear_account(account)?;

    let mut mapping_account = load_account_as_mut::<pc_map_table_t>(account)?;
    mapping_account.magic_ = PC_MAGIC;
    mapping_account.ver_ = version;
    mapping_account.type_ = PC_ACCTYPE_MAPPING;
    mapping_account.size_ =
        try_convert(size_of::<pc_map_table_t>() - size_of_val(&mapping_account.prod_))?;

    Ok(())
}

/// Initialize account as a new product account. This function will zero out any existing data in
/// the account.
pub fn initialize_product_account(account: &AccountInfo, version: u32) -> Result<(), ProgramError> {
    clear_account(account)?;

    let mut prod_account = load_account_as_mut::<pc_prod_t>(account)?;
    prod_account.magic_ = PC_MAGIC;
    prod_account.ver_ = version;
    prod_account.type_ = PC_ACCTYPE_PRODUCT;
    prod_account.size_ = try_convert(size_of::<pc_prod_t>())?;

    Ok(())
}

/// Mutably borrow the data in `account` as a product account, validating that the account
/// is properly formatted. Any mutations to the returned value will be reflected in the
/// account data. Use this to read already-initialized accounts.
pub fn load_product_account_mut<'a>(
    account: &'a AccountInfo,
    expected_version: u32,
) -> Result<RefMut<'a, pc_prod_t>, ProgramError> {
    let product_data = load_account_as_mut::<pc_prod_t>(account)?;

    pyth_assert(
        product_data.magic_ == PC_MAGIC
            && product_data.ver_ == expected_version
            && product_data.type_ == PC_ACCTYPE_PRODUCT,
        ProgramError::InvalidArgument,
    )?;

    Ok(product_data)
}

/// Mutably borrow the data in `account` as a price account, validating that the account
/// is properly formatted. Any mutations to the returned value will be reflected in the
/// account data. Use this to read already-initialized accounts.
fn load_price_account_mut<'a>(
    account: &'a AccountInfo,
    expected_version: u32,
) -> Result<RefMut<'a, pc_price_t>, ProgramError> {
    let price_data = load_account_as_mut::<pc_price_t>(account)?;

    pyth_assert(
        price_data.magic_ == PC_MAGIC
            && price_data.ver_ == expected_version
            && price_data.type_ == PC_ACCTYPE_PRICE,
        ProgramError::InvalidArgument,
    )?;

    Ok(price_data)
}


// Assign pubkey bytes from source to target, fails if source is not 32 bytes
pub fn pubkey_assign(target: &mut pc_pub_key_t, source: &[u8]) {
    unsafe { target.k1_.copy_from_slice(source) }
}

fn pubkey_is_zero(key: &pc_pub_key_t) -> bool {
    return unsafe { key.k8_.iter().all(|x| *x == 0) };
}

fn pubkey_equal(key1: &pc_pub_key_t, key2: &pc_pub_key_t) -> bool {
    return unsafe { key1.k1_.iter().zip(&key2.k1_).all(|(x, y)| *x == *y) };
}
/// Convert `x: T` into a `U`, returning the appropriate `OracleError` if the conversion fails.
fn try_convert<T, U: TryFrom<T>>(x: T) -> Result<U, OracleError> {
    // Note: the error here assumes we're only applying this function to integers right now.
    U::try_from(x).map_err(|_| OracleError::IntegerCastingError)
}
