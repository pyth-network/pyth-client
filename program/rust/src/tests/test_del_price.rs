use std::mem::size_of;

use solana_program::program_error::ProgramError;
use solana_program::pubkey::Pubkey;

use crate::c_oracle_header::{
    cmd_hdr,
    command_t_e_cmd_del_price,
    pc_price_t,
    pc_prod_t,
    PC_VERSION,
};
use crate::deserialize::{
    initialize_pyth_account_checked,
    load_checked,
    load_mut,
};
use crate::rust_oracle::del_price;
use crate::tests::test_utils::AccountSetup;
use crate::utils::pubkey_assign;

#[test]
fn test_del_price() {
    let program_id = Pubkey::new_unique();
    let mut instruction_data = [0u8; size_of::<cmd_hdr>()];
    let mut hdr = load_mut::<cmd_hdr>(&mut instruction_data).unwrap();
    hdr.ver_ = PC_VERSION;
    hdr.cmd_ = command_t_e_cmd_del_price as i32;

    let mut funding_setup = AccountSetup::new_funding();
    let funding_account = funding_setup.to_account_info();

    let mut product_setup = AccountSetup::new::<pc_prod_t>(&program_id);
    let product_account = product_setup.to_account_info();
    initialize_pyth_account_checked::<pc_prod_t>(&product_account, PC_VERSION).unwrap();

    let mut price_setup = AccountSetup::new::<pc_price_t>(&program_id);
    let price_account = price_setup.to_account_info();
    initialize_pyth_account_checked::<pc_price_t>(&price_account, PC_VERSION).unwrap();

    let mut system_setup = AccountSetup::new_system_program();
    let system_account = system_setup.to_account_info();

    // Try deleting a price account that isn't linked to the given product account
    assert_eq!(
        del_price(
            &program_id,
            &[
                funding_account.clone(),
                product_account.clone(),
                price_account.clone(),
                system_account.clone()
            ],
            &instruction_data
        ),
        Err(ProgramError::InvalidArgument)
    );

    // Same test with a random nonzero pubkey
    {
        let mut product_data = load_checked::<pc_prod_t>(&product_account, PC_VERSION).unwrap();
        pubkey_assign(&mut product_data.px_acc_, &Pubkey::new_unique().to_bytes());
    }
    assert_eq!(
        del_price(
            &program_id,
            &[
                funding_account.clone(),
                product_account.clone(),
                price_account.clone(),
                system_account.clone()
            ],
            &instruction_data
        ),
        Err(ProgramError::InvalidArgument)
    );

    // Note that we can't test success outside of the solana vm because of the system program.
}
