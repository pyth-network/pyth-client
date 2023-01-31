use {
    crate::{
        accounts::{
            account_has_key_values,
            clear_account,
            create_pc_str_t,
            MappingAccount,
            ProductAccount,
            PythAccount,
        },
        c_oracle_header::{
            PC_ACCTYPE_PRODUCT,
            PC_MAGIC,
            PC_MAP_TABLE_SIZE,
            PC_PROD_ACC_SIZE,
            PC_VERSION,
        },
        deserialize::{
            load_checked,
            load_mut,
        },
        error::OracleError,
        instruction::{
            CommandHeader,
            OracleCommand,
        },
        processor::process_instruction,
        tests::test_utils::AccountSetup,
    },
    solana_program::{
        account_info::AccountInfo,
        clock::Epoch,
        program_error::ProgramError,
        pubkey::Pubkey,
        rent::Rent,
    },
    std::mem::size_of,
};


#[test]
fn test_add_product() {
    let mut instruction_data = [0u8; PC_PROD_ACC_SIZE as usize];

    let program_id = Pubkey::new_unique();

    let mut funding_setup = AccountSetup::new_funding();
    let funding_account = funding_setup.as_account_info();

    let mut mapping_setup = AccountSetup::new::<MappingAccount>(&program_id);
    let mapping_account = mapping_setup.as_account_info();
    MappingAccount::initialize(&mapping_account, PC_VERSION).unwrap();

    let mut product_setup = AccountSetup::new::<ProductAccount>(&program_id);
    let product_account = product_setup.as_account_info();

    let mut product_setup_2 = AccountSetup::new::<ProductAccount>(&program_id);
    let product_account_2 = product_setup_2.as_account_info();

    let mut size = populate_instruction(&mut instruction_data, &[]);
    assert!(process_instruction(
        &program_id,
        &[
            funding_account.clone(),
            mapping_account.clone(),
            product_account.clone()
        ],
        &instruction_data[..size]
    )
    .is_ok());

    {
        let product_data = load_checked::<ProductAccount>(&product_account, PC_VERSION).unwrap();
        let mapping_data = load_checked::<MappingAccount>(&mapping_account, PC_VERSION).unwrap();

        assert_eq!(product_data.header.magic_number, PC_MAGIC);
        assert_eq!(product_data.header.version, PC_VERSION);
        assert_eq!(product_data.header.account_type, PC_ACCTYPE_PRODUCT);
        assert_eq!(product_data.header.size, size_of::<ProductAccount>() as u32);
        assert_eq!(mapping_data.number_of_products, 1);
        assert_eq!(
            mapping_data.header.size,
            (MappingAccount::INITIAL_SIZE + 32)
        );
        assert!(mapping_data.products_list[0] == *product_account.key);
    }
    assert!(account_has_key_values(&product_account, &[]).unwrap());

    size = populate_instruction(&mut instruction_data, &["foo", "bar"]);
    // Add product with metadata
    assert!(process_instruction(
        &program_id,
        &[
            funding_account.clone(),
            mapping_account.clone(),
            product_account_2.clone()
        ],
        &instruction_data[..size]
    )
    .is_ok());
    {
        let _product_data = load_checked::<ProductAccount>(&product_account_2, PC_VERSION).unwrap();
        let mapping_data = load_checked::<MappingAccount>(&mapping_account, PC_VERSION).unwrap();
        assert_eq!(mapping_data.number_of_products, 2);
        assert_eq!(
            mapping_data.header.size,
            (MappingAccount::INITIAL_SIZE + 2 * 32)
        );
        assert!(mapping_data.products_list[1] == *product_account_2.key);
    }
    assert!(account_has_key_values(&product_account_2, &["foo", "bar"]).unwrap());

    // invalid account size
    let product_key_3 = Pubkey::new_unique();
    let mut product_balance_3 = Rent::minimum_balance(&Rent::default(), PC_PROD_ACC_SIZE as usize);
    let mut prod_raw_data_3 = [0u8; PC_PROD_ACC_SIZE as usize - 1];
    let product_account_3 = AccountInfo::new(
        &product_key_3,
        true,
        true,
        &mut product_balance_3,
        &mut prod_raw_data_3,
        &program_id,
        false,
        Epoch::default(),
    );

    assert_eq!(
        process_instruction(
            &program_id,
            &[
                funding_account.clone(),
                mapping_account.clone(),
                product_account_3.clone()
            ],
            &instruction_data[..size]
        ),
        Err(OracleError::AccountTooSmall.into())
    );

    // test fill up of mapping table
    clear_account(&mapping_account).unwrap();
    MappingAccount::initialize(&mapping_account, PC_VERSION).unwrap();

    for i in 0..PC_MAP_TABLE_SIZE {
        clear_account(&product_account).unwrap();
        size = populate_instruction(&mut instruction_data, &["symbol", &i.to_string()[..]]);

        assert!(process_instruction(
            &program_id,
            &[
                funding_account.clone(),
                mapping_account.clone(),
                product_account.clone()
            ],
            &instruction_data[..size]
        )
        .is_ok());
        let mapping_data = load_checked::<MappingAccount>(&mapping_account, PC_VERSION).unwrap();
        assert_eq!(
            mapping_data.header.size,
            MappingAccount::INITIAL_SIZE + (i + 1) * 32
        );
        assert_eq!(mapping_data.number_of_products, i + 1);
        assert!(account_has_key_values(&product_account, &["symbol", &i.to_string()[..]]).unwrap());
    }

    clear_account(&product_account).unwrap();

    assert_eq!(
        process_instruction(
            &program_id,
            &[
                funding_account.clone(),
                mapping_account.clone(),
                product_account.clone()
            ],
            &instruction_data[..size]
        ),
        Err(ProgramError::InvalidArgument)
    );

    let mapping_data = load_checked::<MappingAccount>(&mapping_account, PC_VERSION).unwrap();
    assert_eq!(mapping_data.number_of_products, PC_MAP_TABLE_SIZE);
}


// Create an add_product instruction that sets the product metadata to strings
pub fn populate_instruction(instruction_data: &mut [u8], strings: &[&str]) -> usize {
    {
        let hdr = load_mut::<CommandHeader>(instruction_data).unwrap();
        *hdr = OracleCommand::AddProduct.into();
    }

    let mut idx = size_of::<CommandHeader>();
    for s in strings.iter() {
        let pc_str = create_pc_str_t(s);
        instruction_data[idx..(idx + pc_str.len())].copy_from_slice(pc_str.as_slice());
        idx += pc_str.len()
    }

    idx
}
