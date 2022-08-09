use std::mem::size_of;

use solana_program::account_info::AccountInfo;
use solana_program::clock::Epoch;
use solana_program::native_token::LAMPORTS_PER_SOL;
use solana_program::program_error::ProgramError;
use solana_program::pubkey::Pubkey;
use solana_program::rent::Rent;
use solana_program::system_program;

use crate::c_oracle_header::{
    cmd_set_min_pub_t,
    command_t_e_cmd_set_min_pub,
    pc_price_t,
    PC_VERSION,
};
use crate::deserialize::load_mut;
use crate::rust_oracle::{
    initialize_checked,
    load_checked,
    set_min_pub,
};

#[test]
fn test_set_min_pub() {
    let mut instruction_data = [0u8; size_of::<cmd_set_min_pub_t>()];

    let program_id = Pubkey::new_unique();
    let funding_key = Pubkey::new_unique();
    let price_key = Pubkey::new_unique();

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

    let mut price_balance = Rent::minimum_balance(&Rent::default(), size_of::<pc_price_t>());
    let mut price_raw_data = [0u8; size_of::<pc_price_t>()];
    let price_account = AccountInfo::new(
        &price_key,
        true,
        true,
        &mut price_balance,
        &mut price_raw_data,
        &program_id,
        false,
        Epoch::default(),
    );

    initialize_checked::<pc_price_t>(&price_account, PC_VERSION).unwrap();

    populate_instruction(&mut instruction_data, 10);
    assert!(set_min_pub(
        &program_id,
        &[funding_account.clone(), price_account.clone()],
        &instruction_data
    )
    .is_ok());
    assert_eq!(get_min_pub(&price_account), Ok(10));

    populate_instruction(&mut instruction_data, 2);
    assert!(set_min_pub(
        &program_id,
        &[funding_account.clone(), price_account.clone()],
        &instruction_data
    )
    .is_ok());
    assert_eq!(get_min_pub(&price_account), Ok(2));
}

// Create an upd_product instruction that sets the product metadata to strings
fn populate_instruction(instruction_data: &mut [u8], min_pub: u8) -> () {
    let mut hdr = load_mut::<cmd_set_min_pub_t>(instruction_data).unwrap();
    hdr.ver_ = PC_VERSION;
    hdr.cmd_ = command_t_e_cmd_set_min_pub as i32;
    hdr.min_pub_ = min_pub;
}

fn get_min_pub(account: &AccountInfo) -> Result<u8, ProgramError> {
    Ok(load_checked::<pc_price_t>(account, PC_VERSION)?.min_pub_)
}
