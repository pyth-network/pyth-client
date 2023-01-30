use {
    crate::{
        accounts::{
            account_has_key_values,
            create_pc_str_t,
            ProductAccount,
            PythAccount,
        },
        c_oracle_header::{
            PC_PROD_ACC_SIZE,
            PC_VERSION,
        },
        deserialize::{
            load_checked,
            load_mut,
        },
        instruction::{
            CommandHeader,
            OracleCommand,
        },
        processor::process_instruction,
        tests::test_utils::AccountSetup,
    },
    solana_program::{
        program_error::ProgramError,
        pubkey::Pubkey,
    },
    std::mem::size_of,
};

#[test]
fn test_upd_product() {
    let mut instruction_data = [0u8; PC_PROD_ACC_SIZE as usize];

    let program_id = Pubkey::new_unique();

    let mut funding_setup = AccountSetup::new_funding();
    let funding_account = funding_setup.as_account_info();

    let mut product_setup = AccountSetup::new::<ProductAccount>(&program_id);
    let product_account = product_setup.as_account_info();

    ProductAccount::initialize(&product_account, PC_VERSION).unwrap();

    let kvs = ["foo", "barz"];
    let size = populate_instruction(&mut instruction_data, &kvs);
    assert!(process_instruction(
        &program_id,
        &[funding_account.clone(), product_account.clone()],
        &instruction_data[..size]
    )
    .is_ok());
    assert!(account_has_key_values(&product_account, &kvs).unwrap_or(false));

    {
        let product_data = load_checked::<ProductAccount>(&product_account, PC_VERSION).unwrap();
        assert_eq!(product_data.header.size, ProductAccount::INITIAL_SIZE + 9);
    }

    // bad size on the 1st string in the key-value pair list
    instruction_data[size_of::<CommandHeader>()] = 2;
    assert_eq!(
        process_instruction(
            &program_id,
            &[funding_account.clone(), product_account.clone()],
            &instruction_data[..size]
        ),
        Err(ProgramError::InvalidArgument)
    );
    assert!(account_has_key_values(&product_account, &kvs).unwrap_or(false));

    let kvs = [];
    let size = populate_instruction(&mut instruction_data, &kvs);
    assert!(process_instruction(
        &program_id,
        &[funding_account.clone(), product_account.clone()],
        &instruction_data[..size]
    )
    .is_ok());
    assert!(account_has_key_values(&product_account, &kvs).unwrap_or(false));
    {
        let product_data = load_checked::<ProductAccount>(&product_account, PC_VERSION).unwrap();
        assert_eq!(product_data.header.size, ProductAccount::INITIAL_SIZE);
    }

    // uneven number of keys and values
    let bad_kvs = ["foo", "bar", "baz"];
    let size = populate_instruction(&mut instruction_data, &bad_kvs);
    assert_eq!(
        process_instruction(
            &program_id,
            &[funding_account.clone(), product_account.clone()],
            &instruction_data[..size]
        ),
        Err(ProgramError::InvalidArgument)
    );
    assert!(account_has_key_values(&product_account, &kvs).unwrap_or(false));
}

// Create an upd_product instruction that sets the product metadata to strings
fn populate_instruction(instruction_data: &mut [u8], strings: &[&str]) -> usize {
    {
        let hdr = load_mut::<CommandHeader>(instruction_data).unwrap();
        *hdr = OracleCommand::UpdProduct.into();
    }

    let mut idx = size_of::<CommandHeader>();
    for s in strings.iter() {
        let pc_str = create_pc_str_t(s);
        instruction_data[idx..(idx + pc_str.len())].copy_from_slice(pc_str.as_slice());
        idx += pc_str.len()
    }

    idx
}
