use {
    crate::{
        c_oracle_header::{
            MappingAccount,
            PermissionAccount,
            PriceAccount,
            ProductAccount,
            PC_VERSION,
        },
        deserialize::initialize_pyth_account_checked,
        error::OracleError,
        instruction::{
            AddPriceArgs,
            AddPublisherArgs,
            CommandHeader,
            DelPublisherArgs,
            InitPriceArgs,
            OracleCommand::{
                AddMapping,
                AddPrice,
                AddProduct,
                AddPublisher,
                DelPrice,
                DelProduct,
                DelPublisher,
                InitMapping,
                InitPrice,
                ResizePriceAccount,
                SetMinPub,
                UpdProduct,
            },
            SetMinPubArgs,
        },
        processor::process_instruction,
        tests::test_utils::AccountSetup,
    },
    bytemuck::bytes_of,
    solana_program::pubkey::Pubkey,
};

#[test]
fn test_permission_migration() {
    let program_id = Pubkey::new_unique();

    let mut permissions_setup = AccountSetup::new_permission(&program_id);
    let permissions_account = permissions_setup.as_account_info();

    let mut funding_setup = AccountSetup::new_funding();
    let funding_account = funding_setup.as_account_info();

    let mut attacker_setup = AccountSetup::new_funding();
    let attacker_account = attacker_setup.as_account_info();

    let mut mapping_setup = AccountSetup::new::<MappingAccount>(&program_id);
    let mut mapping_account = mapping_setup.as_account_info();

    let mut next_mapping_setup = AccountSetup::new::<MappingAccount>(&program_id);
    let mut next_mapping_account = next_mapping_setup.as_account_info();

    let mut product_setup = AccountSetup::new::<ProductAccount>(&program_id);
    let mut product_account = product_setup.as_account_info();

    let mut price_setup = AccountSetup::new::<PriceAccount>(&program_id);
    let mut price_account = price_setup.as_account_info();


    product_account.is_signer = false;
    mapping_account.is_signer = false;
    price_account.is_signer = false;
    next_mapping_account.is_signer = false;


    {
        let mut permissions_account_data =
            initialize_pyth_account_checked::<PermissionAccount>(&permissions_account, PC_VERSION)
                .unwrap();
        permissions_account_data.master_authority = *funding_account.key;
    }

    assert_eq!(
        process_instruction(
            &program_id,
            &[
                attacker_account.clone(),
                mapping_account.clone(),
                next_mapping_account.clone(),
                permissions_account.clone()
            ],
            bytes_of::<CommandHeader>(&AddMapping.into())
        ),
        Err(OracleError::PermissionViolation.into())
    );


    process_instruction(
        &program_id,
        &[
            funding_account.clone(),
            mapping_account.clone(),
            permissions_account.clone(),
        ],
        bytes_of::<CommandHeader>(&InitMapping.into()),
    )
    .unwrap();

    assert_eq!(
        process_instruction(
            &program_id,
            &[
                funding_account.clone(),
                mapping_account.clone(),
                next_mapping_account.clone(),
                permissions_account.clone()
            ],
            bytes_of::<CommandHeader>(&AddMapping.into())
        ),
        Err(OracleError::PermissionViolation.into())
    );

    assert_eq!(
        process_instruction(
            &program_id,
            &[
                funding_account.clone(),
                mapping_account.clone(),
                product_account.clone(),
                permissions_account.clone()
            ],
            bytes_of::<CommandHeader>(&AddProduct.into())
        ),
        Err(OracleError::PermissionViolation.into())
    );

    assert_eq!(
        process_instruction(
            &program_id,
            &[
                funding_account.clone(),
                product_account.clone(),
                permissions_account.clone()
            ],
            bytes_of::<CommandHeader>(&UpdProduct.into())
        ),
        Err(OracleError::PermissionViolation.into())
    );

    assert_eq!(
        process_instruction(
            &program_id,
            &[
                funding_account.clone(),
                product_account.clone(),
                price_account.clone(),
                permissions_account.clone()
            ],
            bytes_of::<AddPriceArgs>(&AddPriceArgs {
                header:     AddPrice.into(),
                exponent:   -8,
                price_type: 1,
            })
        ),
        Err(OracleError::PermissionViolation.into())
    );

    assert_eq!(
        process_instruction(
            &program_id,
            &[
                funding_account.clone(),
                price_account.clone(),
                permissions_account.clone()
            ],
            bytes_of::<InitPriceArgs>(&InitPriceArgs {
                header:     InitPrice.into(),
                exponent:   -8,
                price_type: 1,
            })
        ),
        Err(OracleError::PermissionViolation.into())
    );

    assert_eq!(
        process_instruction(
            &program_id,
            &[
                funding_account.clone(),
                price_account.clone(),
                permissions_account.clone()
            ],
            bytes_of::<AddPublisherArgs>(&AddPublisherArgs {
                header:    AddPublisher.into(),
                publisher: Pubkey::new_unique(),
            })
        ),
        Err(OracleError::PermissionViolation.into())
    );

    assert_eq!(
        process_instruction(
            &program_id,
            &[
                funding_account.clone(),
                price_account.clone(),
                permissions_account.clone()
            ],
            bytes_of::<DelPublisherArgs>(&DelPublisherArgs {
                header:    DelPublisher.into(),
                publisher: Pubkey::new_unique(),
            })
        ),
        Err(OracleError::PermissionViolation.into())
    );

    assert_eq!(
        process_instruction(
            &program_id,
            &[
                funding_account.clone(),
                price_account.clone(),
                permissions_account.clone()
            ],
            bytes_of::<SetMinPubArgs>(&SetMinPubArgs {
                header:             SetMinPub.into(),
                minimum_publishers: 3,
                unused_:            [0; 3],
            })
        ),
        Err(OracleError::PermissionViolation.into())
    );

    assert_eq!(
        process_instruction(
            &program_id,
            &[
                funding_account.clone(),
                price_account.clone(),
                price_account.clone(), // Mock system program
                permissions_account.clone()
            ],
            bytes_of::<CommandHeader>(&ResizePriceAccount.into())
        ),
        Err(OracleError::PermissionViolation.into())
    );

    assert_eq!(
        process_instruction(
            &program_id,
            &[
                funding_account.clone(),
                product_account.clone(),
                price_account.clone(),
                permissions_account.clone()
            ],
            bytes_of::<CommandHeader>(&DelPrice.into())
        ),
        Err(OracleError::PermissionViolation.into())
    );

    assert_eq!(
        process_instruction(
            &program_id,
            &[
                funding_account.clone(),
                mapping_account.clone(),
                product_account.clone(),
                permissions_account.clone()
            ],
            bytes_of::<CommandHeader>(&DelProduct.into())
        ),
        Err(OracleError::PermissionViolation.into())
    );
}
