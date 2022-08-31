use bytemuck::bytes_of;
use solana_program::program_error::ProgramError;
use solana_program::pubkey::Pubkey;

use crate::c_oracle_header::{
    PriceAccount,
    MAX_NUM_DECIMALS,
    PC_VERSION,
};
use crate::deserialize::{
    initialize_pyth_account_checked,
    load_checked,
};
use crate::instruction::{
    InitPriceArgs,
    OracleCommand,
};
use crate::processor::process_instruction;
use crate::tests::test_utils::AccountSetup;
use crate::OracleError;

#[test]
fn test_init_price() {
    let ptype = 3;

    let cmd: InitPriceArgs = InitPriceArgs {
        header:     OracleCommand::InitPrice.into(),
        exponent:   -2,
        price_type: ptype,
    };

    let instruction_data = bytes_of::<InitPriceArgs>(&cmd);

    let program_id = Pubkey::new_unique();
    let publisher = Pubkey::new_unique();
    let publisher2 = Pubkey::new_unique();
    let product = Pubkey::new_unique();
    let next_price = Pubkey::new_unique();

    let mut funding_setup = AccountSetup::new_funding();
    let funding_account = funding_setup.to_account_info();

    let mut price_setup = AccountSetup::new::<PriceAccount>(&program_id);
    let mut price_account = price_setup.to_account_info();

    // Price account must be initialized
    assert_eq!(
        process_instruction(
            &program_id,
            &[funding_account.clone(), price_account.clone()],
            instruction_data
        ),
        Err(ProgramError::InvalidArgument)
    );

    initialize_pyth_account_checked::<PriceAccount>(&price_account, PC_VERSION).unwrap();
    {
        let mut price_data = load_checked::<PriceAccount>(&price_account, PC_VERSION).unwrap();
        price_data.price_type = ptype;
        price_data.exponent = 0;
        price_data.min_pub_ = 7;
        price_data.num_ = 4;
        price_data.comp_[0].pub_ = publisher;
        price_data.comp_[3].pub_ = publisher2;
        price_data.product_account = product;
        price_data.next_price_account = next_price;

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

    assert!(process_instruction(
        &program_id,
        &[funding_account.clone(), price_account.clone()],
        instruction_data
    )
    .is_ok());

    {
        let price_data = load_checked::<PriceAccount>(&price_account, PC_VERSION).unwrap();

        assert_eq!(price_data.exponent, -2);
        assert_eq!(price_data.price_type, ptype);
        assert_eq!(price_data.min_pub_, 7);
        assert_eq!(price_data.num_, 4);
        assert!(price_data.comp_[0].pub_ == publisher);
        assert!(price_data.comp_[3].pub_ == publisher2);
        assert!(price_data.product_account == product);
        assert!(price_data.next_price_account == next_price);

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
        process_instruction(
            &program_id,
            &[funding_account.clone(), price_account.clone()],
            instruction_data
        ),
        Err(OracleError::InvalidSignableAccount.into())
    );

    price_account.is_signer = true;
    let cmd: InitPriceArgs = InitPriceArgs {
        header:     OracleCommand::InitPrice.into(),
        exponent:   -MAX_NUM_DECIMALS - 1,
        price_type: ptype,
    };
    let instruction_data = bytes_of::<InitPriceArgs>(&cmd);
    assert_eq!(
        process_instruction(
            &program_id,
            &[funding_account.clone(), price_account.clone()],
            instruction_data
        ),
        Err(ProgramError::InvalidArgument)
    );
}
