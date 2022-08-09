use std::mem::size_of;

use crate::tests::test_utils::AccountSetup;
use bytemuck::bytes_of;
use solana_program::account_info::AccountInfo;
use solana_program::clock::Epoch;
use solana_program::msg;
use solana_program::program_error::ProgramError;
use solana_program::pubkey::Pubkey;
use solana_program::rent::Rent;
use std::mem::swap;

use crate::c_oracle_header::{
    cmd_add_publisher,
    cmd_hdr_t,
    command_t_e_cmd_add_product,
    pc_map_table_t,
    pc_price_t,
    pc_prod_t,
    pc_pub_key_t,
    PythAccount,
    PC_ACCTYPE_PRODUCT,
    PC_COMP_SIZE,
    PC_MAGIC,
    PC_MAP_TABLE_SIZE,
    PC_PROD_ACC_SIZE,
    PC_VERSION,
};
use crate::deserialize::load_account_as;
use crate::rust_oracle::{
    add_product,
    add_publisher,
    clear_account,
    initialize_checked,
    load_checked,
    pubkey_equal,
};

#[test]
fn test_add_publisher() {
    let program_id = Pubkey::new_unique();
    let publisher = pc_pub_key_t::new_unique();

    let mut cmd = cmd_add_publisher {
        ver_: PC_VERSION,
        cmd_: command_t_e_cmd_add_product as i32,
        pub_: publisher,
    };
    let mut instruction_data = bytes_of::<cmd_add_publisher>(&cmd);

    let mut funding_setup = AccountSetup::new_funding();
    let funding_account = funding_setup.to_account_info();

    let mut price_setup = AccountSetup::new::<pc_price_t>(&program_id);
    let price_account = price_setup.to_account_info();
    initialize_checked::<pc_price_t>(&price_account, PC_VERSION).unwrap();


    **price_account.try_borrow_mut_lamports().unwrap() = 100;

    // Expect the instruction to fail, because the price account isn't rent exempt
    assert_eq!(
        add_publisher(
            &program_id,
            &[funding_account.clone(), price_account.clone(),],
            instruction_data
        ),
        Err(ProgramError::InvalidArgument)
    );

    // Now give the price account enough lamports to be rent exempt
    **price_account.try_borrow_mut_lamports().unwrap() =
        Rent::minimum_balance(&Rent::default(), pc_price_t::minimum_size());


    assert!(add_publisher(
        &program_id,
        &[funding_account.clone(), price_account.clone(),],
        instruction_data
    )
    .is_ok());

    {
        let price_data = load_checked::<pc_price_t>(&price_account, PC_VERSION).unwrap();
        assert_eq!(price_data.num_, 1);
        assert!(pubkey_equal(
            &price_data.comp_[0].pub_,
            bytes_of(&publisher)
        ));
    }

    // Can't add twice
    assert_eq!(
        add_publisher(
            &program_id,
            &[funding_account.clone(), price_account.clone(),],
            instruction_data
        ),
        Err(ProgramError::InvalidArgument)
    );

    clear_account(&price_account).unwrap();

    // Bad price account
    assert_eq!(
        add_publisher(
            &program_id,
            &[funding_account.clone(), price_account.clone(),],
            instruction_data
        ),
        Err(ProgramError::InvalidArgument)
    );

    initialize_checked::<pc_price_t>(&price_account, PC_VERSION).unwrap();

    //Fill up price node
    for i in 0..PC_COMP_SIZE {
        cmd.pub_ = pc_pub_key_t::new_unique();
        instruction_data = bytes_of::<cmd_add_publisher>(&cmd);
        assert!(add_publisher(
            &program_id,
            &[funding_account.clone(), price_account.clone(),],
            instruction_data
        )
        .is_ok());


        {
            let price_data = load_checked::<pc_price_t>(&price_account, PC_VERSION).unwrap();
            assert_eq!(price_data.num_, i + 1);
            assert!(pubkey_equal(
                &price_data.comp_[i as usize].pub_,
                bytes_of(&cmd.pub_)
            ));
        }
    }

    cmd.pub_ = pc_pub_key_t::new_unique();
    instruction_data = bytes_of::<cmd_add_publisher>(&cmd);
    assert_eq!(
        add_publisher(
            &program_id,
            &[funding_account.clone(), price_account.clone(),],
            instruction_data
        ),
        Err(ProgramError::InvalidArgument)
    );
}
