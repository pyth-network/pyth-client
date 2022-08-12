use solana_program::account_info::AccountInfo;
use solana_program::program_error::ProgramError;
use solana_program::pubkey::Pubkey;
use solana_program::sysvar::clock::Clock;
use solana_program::sysvar::Sysvar;
use std::mem::size_of;

use crate::c_oracle_header::{
    cmd_upd_price_t,
    command_t_e_cmd_upd_price,
    pc_price_t,
    PC_STATUS_TRADING,
    PC_STATUS_UNKNOWN,
    PC_VERSION,
};

use crate::deserialize::{
    initialize_pyth_account_checked,
    load_checked,
    load_mut,
};
use crate::rust_oracle::upd_price;
use crate::tests::test_utils::AccountSetup;
use crate::utils::pubkey_assign;
#[test]
fn test_upd_price() {
    let mut instruction_data = [0u8; size_of::<cmd_upd_price_t>()];
    populate_instruction(&mut instruction_data, Some(42), Some(2), Some(1));

    let program_id = Pubkey::new_unique();

    let mut funding_setup = AccountSetup::new_funding();
    let funding_account = funding_setup.to_account_info();

    let mut price_setup = AccountSetup::new::<pc_price_t>(&program_id);
    let mut price_account = price_setup.to_account_info();
    price_account.is_signer = false;
    initialize_pyth_account_checked::<pc_price_t>(&price_account, PC_VERSION).unwrap();

    {
        let mut price_data = load_checked::<pc_price_t>(&price_account, PC_VERSION).unwrap();
        price_data.num_ = 1;
        pubkey_assign(
            &mut price_data.comp_[0].pub_,
            &funding_account.key.to_bytes(),
        );
    }

    let mut clock_setup = AccountSetup::new_clock();
    let mut clock_account = clock_setup.to_account_info();
    clock_account.is_signer = false;
    clock_account.is_writable = false;

    update_clock_slot(&mut clock_account, 1);

    assert!(upd_price(
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
        let price_data = load_checked::<pc_price_t>(&price_account, PC_VERSION).unwrap();
        assert_eq!(price_data.comp_[0].latest_.price_, 42);
        assert_eq!(price_data.comp_[0].latest_.conf_, 2);
        assert_eq!(price_data.comp_[0].latest_.pub_slot_, 1);
        assert_eq!(price_data.comp_[0].latest_.status_, PC_STATUS_TRADING);
        assert_eq!(price_data.valid_slot_, 0);
        assert_eq!(price_data.agg_.pub_slot_, 1);
        assert_eq!(price_data.agg_.price_, 0);
        assert_eq!(price_data.agg_.status_, PC_STATUS_UNKNOWN);
    }

    // add some prices for current slot - get rejected
    populate_instruction(&mut instruction_data, Some(43), None, None);

    assert_eq!(
        upd_price(
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

    {
        let price_data = load_checked::<pc_price_t>(&price_account, PC_VERSION).unwrap();
        assert_eq!(price_data.comp_[0].latest_.price_, 42);
        assert_eq!(price_data.comp_[0].latest_.conf_, 2);
        assert_eq!(price_data.comp_[0].latest_.pub_slot_, 1);
        assert_eq!(price_data.comp_[0].latest_.status_, PC_STATUS_TRADING);
        assert_eq!(price_data.valid_slot_, 0);
        assert_eq!(price_data.agg_.pub_slot_, 1);
        assert_eq!(price_data.agg_.price_, 0);
        assert_eq!(price_data.agg_.status_, PC_STATUS_UNKNOWN);
    }

    // add next price in new slot triggering snapshot and aggregate calc
    populate_instruction(&mut instruction_data, Some(81), None, Some(2));
    update_clock_slot(&mut clock_account, 3);

    assert!(upd_price(
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
        let price_data = load_checked::<pc_price_t>(&price_account, PC_VERSION).unwrap();
        assert_eq!(price_data.comp_[0].latest_.price_, 81);
        assert_eq!(price_data.comp_[0].latest_.conf_, 2);
        assert_eq!(price_data.comp_[0].latest_.pub_slot_, 2);
        assert_eq!(price_data.comp_[0].latest_.status_, PC_STATUS_TRADING);
        assert_eq!(price_data.valid_slot_, 1);
        assert_eq!(price_data.agg_.pub_slot_, 3);
        assert_eq!(price_data.agg_.price_, 42);
        assert_eq!(price_data.agg_.status_, PC_STATUS_TRADING);
    }

    // next price doesnt change but slot does
    populate_instruction(&mut instruction_data, None, None, Some(3));
    update_clock_slot(&mut clock_account, 4);
    assert!(upd_price(
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
        let price_data = load_checked::<pc_price_t>(&price_account, PC_VERSION).unwrap();
        assert_eq!(price_data.comp_[0].latest_.price_, 81);
        assert_eq!(price_data.comp_[0].latest_.conf_, 2);
        assert_eq!(price_data.comp_[0].latest_.pub_slot_, 3);
        assert_eq!(price_data.comp_[0].latest_.status_, PC_STATUS_TRADING);
        assert_eq!(price_data.valid_slot_, 3);
        assert_eq!(price_data.agg_.pub_slot_, 4);
        assert_eq!(price_data.agg_.price_, 81);
        assert_eq!(price_data.agg_.status_, PC_STATUS_TRADING);
    }

    // next price doesnt change and neither does aggregate but slot does
    populate_instruction(&mut instruction_data, None, None, Some(4));
    update_clock_slot(&mut clock_account, 5);
    assert!(upd_price(
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
        let price_data = load_checked::<pc_price_t>(&price_account, PC_VERSION).unwrap();
        assert_eq!(price_data.comp_[0].latest_.price_, 81);
        assert_eq!(price_data.comp_[0].latest_.conf_, 2);
        assert_eq!(price_data.comp_[0].latest_.pub_slot_, 4);
        assert_eq!(price_data.comp_[0].latest_.status_, PC_STATUS_TRADING);
        assert_eq!(price_data.valid_slot_, 4);
        assert_eq!(price_data.agg_.pub_slot_, 5);
        assert_eq!(price_data.agg_.price_, 81);
        assert_eq!(price_data.agg_.status_, PC_STATUS_TRADING);
    }

    // try to publish back-in-time
    populate_instruction(&mut instruction_data, None, None, Some(1));
    update_clock_slot(&mut clock_account, 5);
    assert_eq!(
        upd_price(
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

    {
        let price_data = load_checked::<pc_price_t>(&price_account, PC_VERSION).unwrap();
        assert_eq!(price_data.comp_[0].latest_.price_, 81);
        assert_eq!(price_data.comp_[0].latest_.conf_, 2);
        assert_eq!(price_data.comp_[0].latest_.pub_slot_, 4);
        assert_eq!(price_data.comp_[0].latest_.status_, PC_STATUS_TRADING);
        assert_eq!(price_data.valid_slot_, 4);
        assert_eq!(price_data.agg_.pub_slot_, 5);
        assert_eq!(price_data.agg_.price_, 81);
        assert_eq!(price_data.agg_.status_, PC_STATUS_TRADING);
    }

    populate_instruction(&mut instruction_data, Some(50), Some(6), Some(5));
    update_clock_slot(&mut clock_account, 6);

    // Publishing a wide CI results in a status of unknown.

    // check that someone doesn't accidentally break the test.
    assert!(upd_price(
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
        let price_data = load_checked::<pc_price_t>(&price_account, PC_VERSION).unwrap();
        assert_eq!(price_data.comp_[0].latest_.price_, 50);
        assert_eq!(price_data.comp_[0].latest_.conf_, 6);
        assert_eq!(price_data.comp_[0].latest_.pub_slot_, 5);
        assert_eq!(price_data.comp_[0].latest_.status_, PC_STATUS_UNKNOWN);
        assert_eq!(price_data.valid_slot_, 5);
        assert_eq!(price_data.agg_.pub_slot_, 6);
        assert_eq!(price_data.agg_.price_, 81);
        assert_eq!(price_data.agg_.status_, PC_STATUS_TRADING);
    }

    // Crank one more time and aggregate should be unknown
    populate_instruction(&mut instruction_data, None, None, Some(6));
    update_clock_slot(&mut clock_account, 7);

    assert!(upd_price(
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
        let price_data = load_checked::<pc_price_t>(&price_account, PC_VERSION).unwrap();
        assert_eq!(price_data.comp_[0].latest_.price_, 50);
        assert_eq!(price_data.comp_[0].latest_.conf_, 6);
        assert_eq!(price_data.comp_[0].latest_.pub_slot_, 6);
        assert_eq!(price_data.comp_[0].latest_.status_, PC_STATUS_UNKNOWN);
        assert_eq!(price_data.valid_slot_, 6);
        assert_eq!(price_data.agg_.pub_slot_, 7);
        assert_eq!(price_data.agg_.price_, 81);
        assert_eq!(price_data.agg_.status_, PC_STATUS_UNKNOWN);
    }
}

fn update_clock_slot(clock_account: &mut AccountInfo, slot: u64) {
    let mut clock_data = Clock::from_account_info(clock_account).unwrap();
    clock_data.slot = slot;
    clock_data.to_account_info(clock_account);
}

// Create an upd_price instruction that sets the product metadata to strings
fn populate_instruction(
    instruction_data: &mut [u8],
    price: Option<i64>,
    conf: Option<u64>,
    pub_slot: Option<u64>,
) -> () {
    let mut cmd = load_mut::<cmd_upd_price_t>(instruction_data).unwrap();
    cmd.ver_ = PC_VERSION;
    cmd.cmd_ = command_t_e_cmd_upd_price as i32;
    cmd.status_ = PC_STATUS_TRADING;
    if let Some(price_) = price {
        cmd.price_ = price_;
    }
    if let Some(conf_) = conf {
        cmd.conf_ = conf_;
    }
    if let Some(pub_slot_) = pub_slot {
        cmd.pub_slot_ = pub_slot_;
    }
    cmd.unused_ = 0;
}
