use {
    crate::{
        c_oracle_header::{
            MappingAccount,
            PC_MAGIC,
            PC_MAP_TABLE_SIZE,
            PC_VERSION,
        },
        deserialize::{
            initialize_pyth_account_checked,
            load_account_as_mut,
            load_checked,
        },
        error::OracleError,
        instruction::{
            CommandHeader,
            OracleCommand,
        },
        processor::process_instruction,
        tests::test_utils::AccountSetup,
        utils::clear_account,
    },
    bytemuck::bytes_of,
    solana_program::{
        program_error::ProgramError,
        pubkey::Pubkey,
    },
};

#[test]
fn test_add_mapping() {
    let hdr: CommandHeader = OracleCommand::AddMapping.into();
    let instruction_data = bytes_of::<CommandHeader>(&hdr);

    let program_id = Pubkey::new_unique();

    let mut funding_setup = AccountSetup::new_funding();
    let funding_account = funding_setup.as_account_info();

    let mut curr_mapping_setup = AccountSetup::new::<MappingAccount>(&program_id);
    let cur_mapping = curr_mapping_setup.as_account_info();
    initialize_pyth_account_checked::<MappingAccount>(&cur_mapping, PC_VERSION).unwrap();

    let mut next_mapping_setup = AccountSetup::new::<MappingAccount>(&program_id);
    let next_mapping = next_mapping_setup.as_account_info();

    {
        let mut cur_mapping_data =
            load_checked::<MappingAccount>(&cur_mapping, PC_VERSION).unwrap();
        cur_mapping_data.number_of_products = PC_MAP_TABLE_SIZE;
    }

    assert!(process_instruction(
        &program_id,
        &[
            funding_account.clone(),
            cur_mapping.clone(),
            next_mapping.clone()
        ],
        instruction_data
    )
    .is_ok());

    {
        let next_mapping_data = load_checked::<MappingAccount>(&next_mapping, PC_VERSION).unwrap();
        let mut cur_mapping_data =
            load_checked::<MappingAccount>(&cur_mapping, PC_VERSION).unwrap();

        assert!(cur_mapping_data.next_mapping_account == *next_mapping.key);
        assert!(next_mapping_data.next_mapping_account == Pubkey::default());
        cur_mapping_data.next_mapping_account = Pubkey::default();
        cur_mapping_data.number_of_products = 0;
    }

    clear_account(&next_mapping).unwrap();

    assert_eq!(
        process_instruction(
            &program_id,
            &[
                funding_account.clone(),
                cur_mapping.clone(),
                next_mapping.clone()
            ],
            instruction_data
        ),
        Err(ProgramError::InvalidArgument)
    );

    {
        let mut cur_mapping_data =
            load_checked::<MappingAccount>(&cur_mapping, PC_VERSION).unwrap();
        assert!(cur_mapping_data.next_mapping_account == Pubkey::default());
        cur_mapping_data.number_of_products = PC_MAP_TABLE_SIZE;
        cur_mapping_data.header.magic_number = 0;
    }

    assert_eq!(
        process_instruction(
            &program_id,
            &[
                funding_account.clone(),
                cur_mapping.clone(),
                next_mapping.clone()
            ],
            instruction_data
        ),
        Err(OracleError::InvalidAccountHeader.into())
    );

    {
        let mut cur_mapping_data = load_account_as_mut::<MappingAccount>(&cur_mapping).unwrap();
        cur_mapping_data.header.magic_number = PC_MAGIC;
    }

    assert!(process_instruction(
        &program_id,
        &[
            funding_account.clone(),
            cur_mapping.clone(),
            next_mapping.clone()
        ],
        instruction_data
    )
    .is_ok());
}
