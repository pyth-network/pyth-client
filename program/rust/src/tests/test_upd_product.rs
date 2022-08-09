use std::mem::size_of;

use crate::tests::test_utils::AccountSetup;
use solana_program::account_info::AccountInfo;
use solana_program::program_error::ProgramError;
use solana_program::pubkey::Pubkey;

use crate::c_oracle_header::{
    cmd_hdr_t,
    cmd_upd_product_t,
    command_t_e_cmd_upd_product,
    pc_prod_t,
    PC_PROD_ACC_SIZE,
    PC_VERSION,
};
use crate::deserialize::load_mut;
use crate::rust_oracle::{
    initialize_checked,
    load_checked,
    read_pc_str_t,
    try_convert,
    upd_product,
};

#[test]
fn test_upd_product() {
    let mut instruction_data = [0u8; PC_PROD_ACC_SIZE as usize];

    let program_id = Pubkey::new_unique();

    let mut funding_setup = AccountSetup::new_funding();
    let funding_account = funding_setup.to_account_info();

    let mut product_setup = AccountSetup::new::<pc_prod_t>(&program_id);
    let product_account = product_setup.to_account_info();

    initialize_checked::<pc_prod_t>(&product_account, PC_VERSION).unwrap();

    let kvs = ["foo", "barz"];
    let size = populate_instruction(&mut instruction_data, &kvs);
    assert!(upd_product(
        &program_id,
        &[funding_account.clone(), product_account.clone()],
        &instruction_data[..size]
    )
    .is_ok());
    assert!(account_has_key_values(&product_account, &kvs).unwrap_or(false));

    // bad size on the 1st string in the key-value pair list
    instruction_data[size_of::<cmd_upd_product_t>()] = 2;
    assert_eq!(
        upd_product(
            &program_id,
            &[funding_account.clone(), product_account.clone()],
            &instruction_data[..size]
        ),
        Err(ProgramError::InvalidArgument)
    );
    assert!(account_has_key_values(&product_account, &kvs).unwrap_or(false));

    let kvs = [];
    let size = populate_instruction(&mut instruction_data, &kvs);
    assert!(upd_product(
        &program_id,
        &[funding_account.clone(), product_account.clone()],
        &instruction_data[..size]
    )
    .is_ok());
    assert!(account_has_key_values(&product_account, &kvs).unwrap_or(false));

    // uneven number of keys and values
    let bad_kvs = ["foo", "bar", "baz"];
    let size = populate_instruction(&mut instruction_data, &bad_kvs);
    assert_eq!(
        upd_product(
            &program_id,
            &[funding_account.clone(), product_account.clone()],
            &instruction_data[..size]
        ),
        Err(ProgramError::InvalidArgument)
    );
    assert!(account_has_key_values(&product_account, &kvs).unwrap_or(false));
}

// Create an upd_product instruction that sets the product metadata to strings
fn populate_instruction(instruction_data: &mut [u8], strings: &[&str]) -> usize {
    {
        let mut hdr = load_mut::<cmd_hdr_t>(instruction_data).unwrap();
        hdr.ver_ = PC_VERSION;
        hdr.cmd_ = command_t_e_cmd_upd_product as i32
    }

    let mut idx = size_of::<cmd_upd_product_t>();
    for s in strings.iter() {
        let pc_str = create_pc_str_t(s);
        instruction_data[idx..(idx + pc_str.len())].copy_from_slice(pc_str.as_slice());
        idx += pc_str.len()
    }

    idx
}

fn create_pc_str_t(s: &str) -> Vec<u8> {
    let mut v = vec![s.len() as u8];
    v.extend_from_slice(s.as_bytes());
    v
}

// Check that the key-value list in product_account equals the strings in expected
// Returns an Err if the account data is incorrectly formatted and the comparison cannot be
// performed.
fn account_has_key_values(
    product_account: &AccountInfo,
    expected: &[&str],
) -> Result<bool, ProgramError> {
    let account_size: usize =
        try_convert(load_checked::<pc_prod_t>(product_account, PC_VERSION)?.size_)?;
    let mut all_account_data = product_account.try_borrow_mut_data()?;
    let kv_data = &mut all_account_data[size_of::<pc_prod_t>()..account_size];
    let mut kv_idx = 0;
    let mut expected_idx = 0;

    while kv_idx < kv_data.len() {
        let key = read_pc_str_t(&kv_data[kv_idx..])?;
        if key[0] != try_convert::<_, u8>(key.len())? - 1 {
            return Ok(false);
        }

        if &key[1..] != expected[expected_idx].as_bytes() {
            return Ok(false);
        }

        kv_idx += key.len();
        expected_idx += 1;
    }

    if expected_idx != expected.len() {
        return Ok(false);
    }

    Ok(true)
}
