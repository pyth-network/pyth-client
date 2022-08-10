use std::mem::size_of;

use solana_program::account_info::AccountInfo;
use solana_program::program_error::ProgramError;
use solana_program::pubkey::Pubkey;

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
use crate::tests::test_utils::AccountSetup;

#[test]
fn test_set_min_pub() {
    let mut instruction_data = [0u8; size_of::<cmd_set_min_pub_t>()];

    let program_id = Pubkey::new_unique();

    let mut funding_setup = AccountSetup::new_funding();
    let funding_account = funding_setup.to_account_info();

    let mut price_setup = AccountSetup::new::<pc_price_t>(&program_id);
    let price_account = price_setup.to_account_info();
    initialize_checked::<pc_price_t>(&price_account, PC_VERSION).unwrap();

    assert_eq!(get_min_pub(&price_account), Ok(0));

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
