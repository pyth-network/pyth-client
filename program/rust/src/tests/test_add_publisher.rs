use crate::c_oracle_header::{
    pc_price_comp_t,
    pc_price_t,
    pc_pub_key_t,
    PythAccount,
    PC_COMP_SIZE,
    PC_VERSION,
};
use crate::deserialize::{
    initialize_pyth_account_checked,
    load_checked,
};
use crate::instruction::{
    AddPublisherArgs,
    OracleCommand,
};
use crate::processor::process_instruction;
use crate::tests::test_utils::AccountSetup;
use crate::utils::{
    clear_account,
    pubkey_equal,
};
use crate::OracleError;
use bytemuck::bytes_of;
use solana_program::program_error::ProgramError;
use solana_program::pubkey::Pubkey;
use solana_program::rent::Rent;
use std::mem::size_of;

#[test]
fn test_add_publisher() {
    let program_id = Pubkey::new_unique();
    let publisher = pc_pub_key_t::new_unique();

    let mut cmd = AddPublisherArgs {
        header: OracleCommand::AddPublisher.into(),
        publisher,
    };
    let mut instruction_data = bytes_of::<AddPublisherArgs>(&cmd);

    let mut funding_setup = AccountSetup::new_funding();
    let funding_account = funding_setup.to_account_info();

    let mut price_setup = AccountSetup::new::<pc_price_t>(&program_id);
    let price_account = price_setup.to_account_info();
    initialize_pyth_account_checked::<pc_price_t>(&price_account, PC_VERSION).unwrap();


    **price_account.try_borrow_mut_lamports().unwrap() = 100;

    // Expect the instruction to fail, because the price account isn't rent exempt
    assert_eq!(
        process_instruction(
            &program_id,
            &[funding_account.clone(), price_account.clone(),],
            instruction_data
        ),
        Err(OracleError::InvalidSignableAccount.into())
    );

    // Now give the price account enough lamports to be rent exempt
    **price_account.try_borrow_mut_lamports().unwrap() =
        Rent::minimum_balance(&Rent::default(), pc_price_t::minimum_size());


    assert!(process_instruction(
        &program_id,
        &[funding_account.clone(), price_account.clone(),],
        instruction_data
    )
    .is_ok());

    {
        let price_data = load_checked::<pc_price_t>(&price_account, PC_VERSION).unwrap();
        assert_eq!(price_data.num_, 1);
        assert_eq!(
            price_data.size_,
            pc_price_t::INITIAL_SIZE + (size_of::<pc_price_comp_t>() as u32)
        );
        assert!(pubkey_equal(
            &price_data.comp_[0].pub_,
            bytes_of(&publisher)
        ));
    }

    // Can't add twice
    assert_eq!(
        process_instruction(
            &program_id,
            &[funding_account.clone(), price_account.clone(),],
            instruction_data
        ),
        Err(ProgramError::InvalidArgument)
    );

    clear_account(&price_account).unwrap();

    // Bad price account
    assert_eq!(
        process_instruction(
            &program_id,
            &[funding_account.clone(), price_account.clone(),],
            instruction_data
        ),
        Err(ProgramError::InvalidArgument)
    );

    initialize_pyth_account_checked::<pc_price_t>(&price_account, PC_VERSION).unwrap();

    //Fill up price node
    for i in 0..PC_COMP_SIZE {
        cmd.publisher = pc_pub_key_t::new_unique();
        instruction_data = bytes_of::<AddPublisherArgs>(&cmd);
        assert!(process_instruction(
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
                bytes_of(&cmd.publisher)
            ));
            assert_eq!(
                price_data.size_,
                pc_price_t::INITIAL_SIZE + (size_of::<pc_price_comp_t>() as u32) * (i + 1)
            );
        }
    }

    cmd.publisher = pc_pub_key_t::new_unique();
    instruction_data = bytes_of::<AddPublisherArgs>(&cmd);
    assert_eq!(
        process_instruction(
            &program_id,
            &[funding_account.clone(), price_account.clone(),],
            instruction_data
        ),
        Err(ProgramError::InvalidArgument)
    );
}
