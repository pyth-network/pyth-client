use solana_program::program_error::ProgramError;
use solana_program::pubkey::Pubkey;
use std::mem::size_of;

use crate::c_oracle_header::{
    PriceAccount,
    PC_STATUS_TRADING,
    PC_STATUS_UNKNOWN,
    PC_VERSION,
};

use crate::deserialize::{
    initialize_pyth_account_checked,
    load_checked,
    load_mut,
};
use crate::instruction::{
    OracleCommand,
    UpdPriceArgs,
};
use crate::processor::process_instruction;
use crate::tests::test_utils::{
    update_clock_slot,
    AccountSetup,
};
#[test]
fn test_upd_price_no_fail_on_error_no_fail_on_error() {
    let mut instruction_data = [0u8; size_of::<UpdPriceArgs>()];

    let program_id = Pubkey::new_unique();

    let mut funding_setup = AccountSetup::new_funding();
    let funding_account = funding_setup.to_account_info();

    let mut price_setup = AccountSetup::new::<PriceAccount>(&program_id);
    let mut price_account = price_setup.to_account_info();
    price_account.is_signer = false;
    initialize_pyth_account_checked::<PriceAccount>(&price_account, PC_VERSION).unwrap();

    let mut clock_setup = AccountSetup::new_clock();
    let mut clock_account = clock_setup.to_account_info();
    clock_account.is_signer = false;
    clock_account.is_writable = false;

    update_clock_slot(&mut clock_account, 1);


    // Check that the normal upd_price fails
    populate_instruction(&mut instruction_data, 42, 9, 1, true);

    assert_eq!(
        process_instruction(
            &program_id,
            &[
                funding_account.clone(),
                price_account.clone(),
                clock_account.clone()
            ],
            &instruction_data
        ),
        Err(ProgramError::InvalidArgument)
    );


    populate_instruction(&mut instruction_data, 42, 9, 1, false);
    // We haven't permissioned the publish account for the price account
    // yet, so any update should fail silently and have no effect. The
    // transaction should "succeed".
    assert!(process_instruction(
        &program_id,
        &[
            funding_account.clone(),
            price_account.clone(),
            clock_account.clone()
        ],
        &instruction_data
    )
    .is_ok());


    {
        let mut price_data = load_checked::<PriceAccount>(&price_account, PC_VERSION).unwrap();
        assert_eq!(price_data.comp_[0].latest_.price_, 0);
        assert_eq!(price_data.comp_[0].latest_.conf_, 0);
        assert_eq!(price_data.comp_[0].latest_.pub_slot_, 0);
        assert_eq!(price_data.comp_[0].latest_.status_, PC_STATUS_UNKNOWN);
        assert_eq!(price_data.valid_slot_, 0);
        assert_eq!(price_data.agg_.pub_slot_, 0);
        assert_eq!(price_data.agg_.price_, 0);
        assert_eq!(price_data.agg_.status_, PC_STATUS_UNKNOWN);

        // Now permission the publish account for the price account.
        price_data.num_ = 1;
        price_data.comp_[0].pub_ = *funding_account.key;
    }

    // The update should now succeed, and have an effect.
    assert!(process_instruction(
        &program_id,
        &[
            funding_account.clone(),
            price_account.clone(),
            clock_account.clone()
        ],
        &instruction_data
    )
    .is_ok());

    {
        let price_data = load_checked::<PriceAccount>(&price_account, PC_VERSION).unwrap();
        assert_eq!(price_data.comp_[0].latest_.price_, 42);
        assert_eq!(price_data.comp_[0].latest_.conf_, 9);
        assert_eq!(price_data.comp_[0].latest_.pub_slot_, 1);
        assert_eq!(price_data.comp_[0].latest_.status_, PC_STATUS_UNKNOWN);
        assert_eq!(price_data.valid_slot_, 0);
        assert_eq!(price_data.agg_.pub_slot_, 1);
        assert_eq!(price_data.agg_.price_, 0);
        assert_eq!(price_data.agg_.status_, PC_STATUS_UNKNOWN);
    }

    // Invalid updates, such as publishing an update for the current slot,
    // should still fail silently and have no effect.

    // Check that the normal upd_price fails
    populate_instruction(&mut instruction_data, 55, 22, 1, true);
    assert_eq!(
        process_instruction(
            &program_id,
            &[
                funding_account.clone(),
                price_account.clone(),
                clock_account.clone()
            ],
            &instruction_data
        ),
        Err(ProgramError::InvalidArgument)
    );

    populate_instruction(&mut instruction_data, 55, 22, 1, false);
    assert!(process_instruction(
        &program_id,
        &[
            funding_account.clone(),
            price_account.clone(),
            clock_account.clone()
        ],
        &instruction_data
    )
    .is_ok());

    {
        let price_data = load_checked::<PriceAccount>(&price_account, PC_VERSION).unwrap();
        assert_eq!(price_data.comp_[0].latest_.price_, 42);
        assert_eq!(price_data.comp_[0].latest_.conf_, 9);
        assert_eq!(price_data.comp_[0].latest_.pub_slot_, 1);
        assert_eq!(price_data.comp_[0].latest_.status_, PC_STATUS_UNKNOWN);
        assert_eq!(price_data.valid_slot_, 0);
        assert_eq!(price_data.agg_.pub_slot_, 1);
        assert_eq!(price_data.agg_.price_, 0);
        assert_eq!(price_data.agg_.status_, PC_STATUS_UNKNOWN);
    }
}


// Create an upd_price_no_fail_on_error or upd_price instruction with the provided parameters
fn populate_instruction(
    instruction_data: &mut [u8],
    price: i64,
    conf: u64,
    pub_slot: u64,
    fail_on_error: bool,
) -> () {
    let mut cmd = load_mut::<UpdPriceArgs>(instruction_data).unwrap();
    cmd.header = if fail_on_error {
        OracleCommand::UpdPrice.into()
    } else {
        OracleCommand::UpdPriceNoFailOnError.into()
    };
    cmd.status = PC_STATUS_TRADING;
    cmd.price = price;
    cmd.confidence = conf;
    cmd.publishing_slot = pub_slot;
    cmd.unused_ = 0;
}
