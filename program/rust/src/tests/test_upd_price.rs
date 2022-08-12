use bytemuck::bytes_of;
use solana_program::account_info::AccountInfo;
use solana_program::pubkey::Pubkey;
use solana_program::sysvar::clock::Clock;
use solana_program::sysvar::Sysvar;

use crate::c_oracle_header::{
    cmd_upd_price_t,
    command_t_e_cmd_upd_price,
    pc_price_t,
    PC_STATUS_TRADING,
    PC_VERSION,
};

use crate::deserialize::{
    initialize_pyth_account_checked,
    load_checked,
};
use crate::rust_oracle::upd_price;
use crate::tests::test_utils::AccountSetup;
use crate::utils::pubkey_assign;
#[test]
fn test_upd_price() {
    let cmd: cmd_upd_price_t = cmd_upd_price_t {
        ver_:      PC_VERSION,
        cmd_:      command_t_e_cmd_upd_price as i32,
        status_:   PC_STATUS_TRADING,
        price_:    42,
        conf_:     2,
        pub_slot_: 1,
        unused_:   0,
    };

    let instruction_data = bytes_of::<cmd_upd_price_t>(&cmd);

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
        assert_eq!(price_data.agg_.pub_slot_, 1);
        assert_eq!(price_data.valid_slot_, 0)
    }
}

fn update_clock_slot(clock_account: &mut AccountInfo, slot: u64) {
    let mut clock_data = Clock::from_account_info(clock_account).unwrap();
    clock_data.slot = slot;
    clock_data.to_account_info(clock_account);
}
