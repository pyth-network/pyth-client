use std::mem::size_of;

use solana_program::account_info::AccountInfo;
use solana_program::clock::Epoch;
use solana_program::native_token::LAMPORTS_PER_SOL;
use solana_program::pubkey::Pubkey;
use solana_program::rent::Rent;
use solana_program::system_program;

use crate::c_oracle_header::{
    cmd_hdr_t,
    cmd_upd_product_t,
    command_t_e_cmd_upd_product,
    PC_PROD_ACC_SIZE,
    PC_VERSION,
};
use crate::deserialize::load_mut;
use crate::rust_oracle::{
    initialize_product_account,
    upd_product,
};

#[test]
fn test_upd_product() {
    let mut instruction_data = [0u8; PC_PROD_ACC_SIZE as usize];

    let program_id = Pubkey::new_unique();
    let funding_key = Pubkey::new_unique();
    let product_key = Pubkey::new_unique();

    let system_program = system_program::id();
    let mut funding_balance = LAMPORTS_PER_SOL.clone();
    let funding_account = AccountInfo::new(
        &funding_key,
        true,
        true,
        &mut funding_balance,
        &mut [],
        &system_program,
        false,
        Epoch::default(),
    );

    let mut product_balance = Rent::minimum_balance(&Rent::default(), PC_PROD_ACC_SIZE as usize);
    let mut prod_raw_data = [0u8; PC_PROD_ACC_SIZE as usize];
    let product_account = AccountInfo::new(
        &product_key,
        true,
        true,
        &mut product_balance,
        &mut prod_raw_data,
        &program_id,
        false,
        Epoch::default(),
    );

    initialize_product_account(&product_account, PC_VERSION).unwrap();

    let kvs = [];
    let size = write_cmd(&mut instruction_data, &kvs);
    assert!(upd_product(
        &program_id,
        &[funding_account.clone(), product_account.clone()],
        &instruction_data[..size]
    )
    .is_ok());


    let kvs = ["foo", "barz"];
    let size = write_cmd(&mut instruction_data, &kvs);
    assert!(upd_product(
        &program_id,
        &[funding_account.clone(), product_account.clone()],
        &instruction_data[..size]
    )
    .is_ok());

    // bad size on the string
    instruction_data[0] = 7;
    assert!(upd_product(
        &program_id,
        &[funding_account.clone(), product_account.clone()],
        &instruction_data[..size]
    )
    .is_err());

    // uneven number of keys and values
    let kvs = ["foo", "bar", "baz"];
    let size = write_cmd(&mut instruction_data, &kvs);
    assert!(upd_product(
        &program_id,
        &[funding_account.clone(), product_account.clone()],
        &instruction_data[..size]
    )
    .is_err());
}

fn write_cmd(instruction_data: &mut [u8], strings: &[&str]) -> usize {
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

pub fn create_pc_str_t(s: &str) -> Vec<u8> {
    let mut v = vec![s.len() as u8];
    v.extend_from_slice(s.as_bytes());
    v
}
