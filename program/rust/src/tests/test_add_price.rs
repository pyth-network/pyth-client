use {
    crate::{
        accounts::{
            clear_account,
            MappingAccount,
            PriceAccount,
            ProductAccount,
            PythAccount,
        },
        c_oracle_header::PC_VERSION,
        deserialize::load_checked,
        error::OracleError,
        instruction::{
            AddPriceArgs,
            CommandHeader,
            OracleCommand,
        },
        processor::process_instruction,
        tests::test_utils::AccountSetup,
    },
    bytemuck::bytes_of,
    solana_program::{
        program_error::ProgramError,
        pubkey::Pubkey,
    },
};

#[test]
fn test_add_price() {
    let hdr_add_product = OracleCommand::AddProduct.into();

    let mut hdr_add_price = AddPriceArgs {
        header:     OracleCommand::AddPrice.into(),
        exponent:   1,
        price_type: 1,
    };
    let instruction_data_add_product = bytes_of::<CommandHeader>(&hdr_add_product);
    let mut instruction_data_add_price = bytes_of::<AddPriceArgs>(&hdr_add_price);

    let program_id = Pubkey::new_unique();

    let mut funding_setup = AccountSetup::new_funding();
    let funding_account = funding_setup.as_account_info();

    let mut mapping_setup = AccountSetup::new::<MappingAccount>(&program_id);
    let mapping_account = mapping_setup.as_account_info();
    MappingAccount::initialize(&mapping_account, PC_VERSION).unwrap();

    let mut product_setup = AccountSetup::new::<ProductAccount>(&program_id);
    let product_account = product_setup.as_account_info();

    let mut price_setup = AccountSetup::new::<PriceAccount>(&program_id);
    let mut price_account = price_setup.as_account_info();

    let mut price_setup_2 = AccountSetup::new::<PriceAccount>(&program_id);
    let price_account_2 = price_setup_2.as_account_info();

    assert!(process_instruction(
        &program_id,
        &[
            funding_account.clone(),
            mapping_account.clone(),
            product_account.clone()
        ],
        instruction_data_add_product
    )
    .is_ok());

    assert!(process_instruction(
        &program_id,
        &[
            funding_account.clone(),
            product_account.clone(),
            price_account.clone()
        ],
        instruction_data_add_price
    )
    .is_ok());

    {
        let price_data = load_checked::<PriceAccount>(&price_account, PC_VERSION).unwrap();
        let product_data = load_checked::<ProductAccount>(&product_account, PC_VERSION).unwrap();
        assert_eq!(price_data.exponent, 1);
        assert_eq!(price_data.price_type, 1);
        assert!(price_data.product_account == *product_account.key);
        assert!(price_data.next_price_account == Pubkey::default());
        assert!(product_data.first_price_account == *price_account.key);
    }

    assert!(process_instruction(
        &program_id,
        &[
            funding_account.clone(),
            product_account.clone(),
            price_account_2.clone()
        ],
        instruction_data_add_price
    )
    .is_ok());

    {
        let price_data_2 = load_checked::<PriceAccount>(&price_account_2, PC_VERSION).unwrap();
        let product_data = load_checked::<ProductAccount>(&product_account, PC_VERSION).unwrap();
        assert_eq!(price_data_2.exponent, 1);
        assert_eq!(price_data_2.price_type, 1);
        assert!(price_data_2.product_account == *product_account.key);
        assert!(price_data_2.next_price_account == *price_account.key);
        assert!(product_data.first_price_account == *price_account_2.key);
    }

    // Wrong number of accounts
    assert_eq!(
        process_instruction(
            &program_id,
            &[funding_account.clone(), product_account.clone()],
            instruction_data_add_price
        ),
        Err(OracleError::InvalidNumberOfAccounts.into())
    );

    // Price account is already initialized
    assert_eq!(
        process_instruction(
            &program_id,
            &[
                funding_account.clone(),
                product_account.clone(),
                price_account.clone()
            ],
            instruction_data_add_price
        ),
        Err(OracleError::InvalidFreshAccount.into())
    );

    clear_account(&price_account).unwrap();

    // Wrong ptype
    hdr_add_price = AddPriceArgs {
        header:     OracleCommand::AddPrice.into(),
        exponent:   6,
        price_type: 0,
    };
    instruction_data_add_price = bytes_of::<AddPriceArgs>(&hdr_add_price);


    assert_eq!(
        process_instruction(
            &program_id,
            &[
                funding_account.clone(),
                product_account.clone(),
                price_account.clone()
            ],
            instruction_data_add_price
        ),
        Err(ProgramError::InvalidArgument)
    );


    // Price not signing
    hdr_add_price = AddPriceArgs {
        header:     OracleCommand::AddPrice.into(),
        exponent:   6,
        price_type: 1,
    };

    instruction_data_add_price = bytes_of::<AddPriceArgs>(&hdr_add_price);
    price_account.is_signer = false;

    assert_eq!(
        process_instruction(
            &program_id,
            &[
                funding_account.clone(),
                product_account.clone(),
                price_account.clone()
            ],
            instruction_data_add_price
        ),
        Err(OracleError::InvalidSignableAccount.into())
    );

    // Fresh product account
    price_account.is_signer = true;
    clear_account(&product_account).unwrap();


    assert_eq!(
        process_instruction(
            &program_id,
            &[
                funding_account.clone(),
                product_account.clone(),
                price_account.clone()
            ],
            instruction_data_add_price
        ),
        Err(OracleError::InvalidAccountHeader.into())
    );
}
