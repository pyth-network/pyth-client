use bytemuck::bytes_of;
use solana_program::program_error::ProgramError;
use solana_program::pubkey::Pubkey;

use crate::c_oracle_header::{
    cmd_init_price_t,
    command_t_e_cmd_init_price,
    pc_price_t,
    PC_MAX_NUM_DECIMALS,
    PC_VERSION,
};
use crate::rust_oracle::{
    init_price,
    initialize_checked,
    load_checked,
};
use crate::tests::test_utils::AccountSetup;

#[test]
fn test_init_price() {
    let ptype = 3;

    let cmd: cmd_init_price_t = cmd_init_price_t {
        ver_:   PC_VERSION,
        cmd_:   command_t_e_cmd_init_price as i32,
        expo_:  -2,
        ptype_: ptype,
    };

    let instruction_data = bytes_of::<cmd_init_price_t>(&cmd);

    let program_id = Pubkey::new_unique();

    let mut funding_setup = AccountSetup::new_funding();
    let funding_account = funding_setup.to_account_info();

    let mut price_setup = AccountSetup::new::<pc_price_t>(&program_id);
    let mut price_account = price_setup.to_account_info();

    // Price account must be initialized
    assert_eq!(
        init_price(
            &program_id,
            &[funding_account.clone(), price_account.clone()],
            instruction_data
        ),
        Err(ProgramError::InvalidArgument)
    );

    initialize_checked::<pc_price_t>(&price_account, PC_VERSION).unwrap();
    {
        let mut price_data = load_checked::<pc_price_t>(&price_account, PC_VERSION).unwrap();
        price_data.ptype_ = ptype;
        price_data.expo_ = 0;

        price_data.last_slot_ = 100;
        price_data.valid_slot_ = 100;
        price_data.prev_slot_ = 100;
        price_data.prev_price_ = 100;
        price_data.prev_conf_ = 100;
        price_data.prev_timestamp_ = 100;

        price_data.agg_.price_ = 100;
        price_data.agg_.conf_ = 100;
        price_data.agg_.pub_slot_ = 100;

        price_data.twap_.denom_ = 100;
        price_data.twac_.denom_ = 100;

        price_data.comp_[0].agg_.price_ = 100;
        price_data.comp_[0].latest_.price_ = 100;
        price_data.comp_[3].agg_.price_ = 100;
        price_data.comp_[3].latest_.conf_ = 100;
        let num_components = price_data.comp_.len();
        price_data.comp_[num_components - 1].agg_.price_ = 100;
        price_data.comp_[num_components - 1].latest_.price_ = 100;
    }

    assert!(init_price(
        &program_id,
        &[funding_account.clone(), price_account.clone()],
        instruction_data
    )
    .is_ok());

    {
        let price_data = load_checked::<pc_price_t>(&price_account, PC_VERSION).unwrap();

        assert_eq!(price_data.expo_, -2);

        assert_eq!(price_data.last_slot_, 0);
        assert_eq!(price_data.valid_slot_, 0);
        assert_eq!(price_data.prev_slot_, 0);
        assert_eq!(price_data.prev_price_, 0);
        assert_eq!(price_data.prev_conf_, 0);
        assert_eq!(price_data.prev_timestamp_, 0);

        assert_eq!(price_data.agg_.price_, 0);
        assert_eq!(price_data.agg_.conf_, 0);
        assert_eq!(price_data.agg_.pub_slot_, 0);

        assert_eq!(price_data.twap_.denom_, 0);
        assert_eq!(price_data.twac_.denom_, 0);

        assert_eq!(price_data.comp_[0].agg_.price_, 0);
        assert_eq!(price_data.comp_[0].latest_.price_, 0);
        assert_eq!(price_data.comp_[3].agg_.price_, 0);
        assert_eq!(price_data.comp_[3].latest_.conf_, 0);
        let num_components = price_data.comp_.len();
        assert_eq!(price_data.comp_[num_components - 1].agg_.price_, 0);
        assert_eq!(price_data.comp_[num_components - 1].latest_.price_, 0);
    }

    price_account.is_signer = false;
    assert_eq!(
        init_price(
            &program_id,
            &[funding_account.clone(), price_account.clone()],
            instruction_data
        ),
        Err(ProgramError::InvalidArgument)
    );

    price_account.is_signer = false;
    let cmd: cmd_init_price_t = cmd_init_price_t {
        ver_:   PC_VERSION,
        cmd_:   command_t_e_cmd_init_price as i32,
        expo_:  -(PC_MAX_NUM_DECIMALS as i32) - 1,
        ptype_: ptype,
    };
    let instruction_data = bytes_of::<cmd_init_price_t>(&cmd);
    assert_eq!(
        init_price(
            &program_id,
            &[funding_account.clone(), price_account.clone()],
            instruction_data
        ),
        Err(ProgramError::InvalidArgument)
    );
}
