use {
    crate::{
        accounts::{
            PermissionAccount,
            PriceAccount,
            PriceAccountFlags,
            PythAccount,
        },
        c_oracle_header::PC_VERSION,
        deserialize::load_checked,
        instruction::{
            AddPublisherArgs,
            OracleCommand,
        },
        processor::{
            process_instruction,
            ENABLE_ACCUMULATOR_V2,
        },
        tests::test_utils::AccountSetup,
    },
    bytemuck::bytes_of,
    solana_program::{
        pubkey::Pubkey,
        rent::Rent,
    },
};

#[test]
fn test_aggregate_v2() {
    let program_id = Pubkey::new_unique();
    let publisher = Pubkey::new_unique();

    let mut cmd = AddPublisherArgs {
        header: OracleCommand::AddPublisher.into(),
        publisher,
    };
    let mut instruction_data = bytes_of::<AddPublisherArgs>(&cmd);

    let mut funding_setup = AccountSetup::new_funding();
    let funding_account = funding_setup.as_account_info();

    let mut price_setup = AccountSetup::new::<PriceAccount>(&program_id);
    let price_account = price_setup.as_account_info();
    PriceAccount::initialize(&price_account, PC_VERSION).unwrap();

    **price_account.try_borrow_mut_lamports().unwrap() = 100;

    let mut permissions_setup = AccountSetup::new_permission(&program_id);
    let permissions_account = permissions_setup.as_account_info();

    {
        let mut permissions_account_data =
            PermissionAccount::initialize(&permissions_account, PC_VERSION).unwrap();
        permissions_account_data.master_authority = *funding_account.key;
        permissions_account_data.data_curation_authority = *funding_account.key;
        permissions_account_data.security_authority = *funding_account.key;
    }

    // Give the price account enough lamports to be rent exempt
    **price_account.try_borrow_mut_lamports().unwrap() =
        Rent::minimum_balance(&Rent::default(), PriceAccount::MINIMUM_SIZE);

    assert!(process_instruction(
        &program_id,
        &[
            funding_account.clone(),
            price_account.clone(),
            permissions_account.clone(),
        ],
        instruction_data
    )
    .is_ok());

    {
        let price_data = load_checked::<PriceAccount>(&price_account, PC_VERSION).unwrap();
        assert_eq!(price_data.num_, 1);
        assert_eq!(price_data.header.size, PriceAccount::INITIAL_SIZE);
        assert!(price_data.comp_[0].pub_ == publisher);
        // Make sure that v2 aggregation is disabled
        assert!(!price_data.flags.contains(PriceAccountFlags::ACCUMULATOR_V2));
    }

    cmd.publisher = ENABLE_ACCUMULATOR_V2.into();
    instruction_data = bytes_of::<AddPublisherArgs>(&cmd);
    assert!(process_instruction(
        &program_id,
        &[
            funding_account.clone(),
            price_account.clone(),
            permissions_account.clone(),
        ],
        instruction_data
    )
    .is_ok());

    // Make sure that v2 aggregation is enabled
    {
        let price_data = load_checked::<PriceAccount>(&price_account, PC_VERSION).unwrap();
        assert!(price_data.flags.contains(PriceAccountFlags::ACCUMULATOR_V2));
    }
}
