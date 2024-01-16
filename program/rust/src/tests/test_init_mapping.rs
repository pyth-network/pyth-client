use {
    crate::{
        accounts::{
            clear_account,
            MappingAccount,
            PermissionAccount,
            PythAccount,
        },
        c_oracle_header::{
            PC_ACCTYPE_MAPPING,
            PC_MAGIC,
            PC_VERSION,
        },
        deserialize::load_account_as,
        error::OracleError,
        instruction::{
            CommandHeader,
            OracleCommand,
        },
        processor::process_instruction,
        tests::test_utils::AccountSetup,
    },
    bytemuck::bytes_of,
    solana_program::pubkey::Pubkey,
    std::{
        cell::RefCell,
        rc::Rc,
    },
};

#[test]
fn test_init_mapping() {
    let hdr: CommandHeader = OracleCommand::InitMapping.into();
    let instruction_data = bytes_of::<CommandHeader>(&hdr);

    let program_id = Pubkey::new_unique();
    let program_id_2 = Pubkey::new_unique();

    let mut funding_setup = AccountSetup::new_funding();
    let mut funding_account = funding_setup.as_account_info();

    let mut attacker_setup = AccountSetup::new_funding();
    let attacker_account = attacker_setup.as_account_info();

    let mut mapping_setup = AccountSetup::new::<MappingAccount>(&program_id);
    let mut mapping_account = mapping_setup.as_account_info();

    let mut permissions_setup = AccountSetup::new_permission(&program_id);
    let permissions_account = permissions_setup.as_account_info();

    {
        let mut permissions_account_data =
            PermissionAccount::initialize(&permissions_account, PC_VERSION).unwrap();
        permissions_account_data.master_authority = *funding_account.key;
        permissions_account_data.data_curation_authority = *funding_account.key;
        permissions_account_data.security_authority = *funding_account.key;
    }

    assert!(process_instruction(
        &program_id,
        &[
            funding_account.clone(),
            mapping_account.clone(),
            permissions_account.clone(),
        ],
        instruction_data
    )
    .is_ok());

    {
        let mapping_data = load_account_as::<MappingAccount>(&mapping_account).unwrap();

        assert_eq!(mapping_data.header.version, PC_VERSION);
        assert_eq!(mapping_data.header.magic_number, PC_MAGIC);
        assert_eq!(mapping_data.header.account_type, PC_ACCTYPE_MAPPING);
        assert_eq!(mapping_data.header.size, 56);
    }

    assert_eq!(
        process_instruction(
            &program_id,
            &[
                funding_account.clone(),
                mapping_account.clone(),
                permissions_account.clone(),
            ],
            instruction_data
        ),
        Err(OracleError::InvalidFreshAccount.into())
    );

    clear_account(&mapping_account).unwrap();

    assert_eq!(
        process_instruction(&program_id, &[funding_account.clone(),], instruction_data),
        Err(OracleError::InvalidNumberOfAccounts.into())
    );

    funding_account.is_signer = false;

    assert_eq!(
        process_instruction(
            &program_id,
            &[
                funding_account.clone(),
                mapping_account.clone(),
                permissions_account.clone(),
            ],
            instruction_data
        ),
        Err(OracleError::InvalidFundingAccount.into())
    );

    funding_account.is_signer = true;
    funding_account.is_writable = false;

    assert_eq!(
        process_instruction(
            &program_id,
            &[
                funding_account.clone(),
                mapping_account.clone(),
                permissions_account.clone(),
            ],
            instruction_data
        ),
        Err(OracleError::InvalidFundingAccount.into())
    );

    funding_account.is_writable = true;
    mapping_account.is_writable = false;

    assert_eq!(
        process_instruction(
            &program_id,
            &[
                funding_account.clone(),
                mapping_account.clone(),
                permissions_account.clone(),
            ],
            instruction_data
        ),
        Err(OracleError::InvalidWritableAccount.into())
    );

    mapping_account.is_writable = true;
    mapping_account.owner = &program_id_2;

    assert_eq!(
        process_instruction(
            &program_id,
            &[
                funding_account.clone(),
                mapping_account.clone(),
                permissions_account.clone(),
            ],
            instruction_data
        ),
        Err(OracleError::InvalidWritableAccount.into())
    );

    mapping_account.owner = &program_id;
    let prev_data = mapping_account.data;
    mapping_account.data = Rc::new(RefCell::new(&mut []));

    assert_eq!(
        process_instruction(
            &program_id,
            &[
                funding_account.clone(),
                mapping_account.clone(),
                permissions_account.clone(),
            ],
            instruction_data
        ),
        Err(OracleError::AccountTooSmall.into())
    );

    mapping_account.data = prev_data;

    assert!(process_instruction(
        &program_id,
        &[
            funding_account.clone(),
            mapping_account.clone(),
            permissions_account.clone(),
        ],
        instruction_data
    )
    .is_ok());

    clear_account(&mapping_account).unwrap();
    mapping_account.is_signer = false;

    let mut permissions_setup = AccountSetup::new_permission(&program_id);
    let permissions_account = permissions_setup.as_account_info();

    // Permissions account is uninitialized
    assert_eq!(
        process_instruction(
            &program_id,
            &[
                funding_account.clone(),
                mapping_account.clone(),
                permissions_account.clone()
            ],
            instruction_data
        ),
        Err(OracleError::InvalidAccountHeader.into())
    );

    {
        let mut permissions_account_data =
            PermissionAccount::initialize(&permissions_account, PC_VERSION).unwrap();
        permissions_account_data.master_authority = *funding_account.key;
    }

    // Attacker account tries to sign transaction instead of funding account
    assert_eq!(
        process_instruction(
            &program_id,
            &[
                attacker_account.clone(),
                mapping_account.clone(),
                permissions_account.clone()
            ],
            instruction_data
        ),
        Err(OracleError::PermissionViolation.into())
    );

    // Attacker tries to impersonate permissions account
    let mut impersonating_permission_setup = AccountSetup::new::<PermissionAccount>(&program_id);
    let impersonating_permission_account = impersonating_permission_setup.as_account_info();

    {
        let mut impersonating_permission_account_data =
            PermissionAccount::initialize(&impersonating_permission_account, PC_VERSION).unwrap();
        impersonating_permission_account_data.master_authority = *attacker_account.key;
    }

    assert_eq!(
        process_instruction(
            &program_id,
            &[
                attacker_account.clone(),
                mapping_account.clone(),
                impersonating_permission_account.clone()
            ],
            instruction_data
        ),
        Err(OracleError::InvalidPda.into())
    );

    // Right authority, initialized permissions account
    assert!(process_instruction(
        &program_id,
        &[
            funding_account.clone(),
            mapping_account.clone(),
            permissions_account.clone()
        ],
        instruction_data
    )
    .is_ok());
}
