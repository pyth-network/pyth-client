use crate::c_oracle_header::{
    pc_map_table_t,
    PC_ACCTYPE_MAPPING,
    PC_MAGIC,
    PC_VERSION,
};
use crate::deserialize::load_account_as;
use crate::error::OracleError;
use crate::instruction::{
    CommandHeader,
    OracleCommand,
};
use crate::processor::process_instruction;
use crate::tests::test_utils::AccountSetup;
use crate::utils::clear_account;
use bytemuck::bytes_of;
use solana_program::program_error::ProgramError;
use solana_program::pubkey::Pubkey;
use std::cell::RefCell;
use std::rc::Rc;

#[test]
fn test_init_mapping() {
    let hdr: CommandHeader = OracleCommand::InitMapping.into();
    let instruction_data = bytes_of::<CommandHeader>(&hdr);

    let program_id = Pubkey::new_unique();
    let program_id_2 = Pubkey::new_unique();

    let mut funding_setup = AccountSetup::new_funding();
    let mut funding_account = funding_setup.to_account_info();

    let mut mapping_setup = AccountSetup::new::<pc_map_table_t>(&program_id);
    let mut mapping_account = mapping_setup.to_account_info();

    assert!(process_instruction(
        &program_id,
        &[funding_account.clone(), mapping_account.clone()],
        instruction_data
    )
    .is_ok());

    {
        let mapping_data = load_account_as::<pc_map_table_t>(&mapping_account).unwrap();

        assert_eq!(mapping_data.ver_, PC_VERSION);
        assert_eq!(mapping_data.magic_, PC_MAGIC);
        assert_eq!(mapping_data.type_, PC_ACCTYPE_MAPPING);
        assert_eq!(mapping_data.size_, 56);
    }

    assert_eq!(
        process_instruction(
            &program_id,
            &[funding_account.clone(), mapping_account.clone()],
            instruction_data
        ),
        Err(OracleError::InvalidFreshAccount.into())
    );

    clear_account(&mapping_account).unwrap();

    assert_eq!(
        process_instruction(&program_id, &[funding_account.clone()], instruction_data),
        Err(ProgramError::InvalidArgument)
    );

    funding_account.is_signer = false;

    assert_eq!(
        process_instruction(
            &program_id,
            &[funding_account.clone(), mapping_account.clone()],
            instruction_data
        ),
        Err(OracleError::InvalidFundingAccount.into())
    );

    funding_account.is_signer = true;
    mapping_account.is_signer = false;

    assert_eq!(
        process_instruction(
            &program_id,
            &[funding_account.clone(), mapping_account.clone()],
            instruction_data
        ),
        Err(OracleError::InvalidSignableAccount.into())
    );

    mapping_account.is_signer = true;
    funding_account.is_writable = false;

    assert_eq!(
        process_instruction(
            &program_id,
            &[funding_account.clone(), mapping_account.clone()],
            instruction_data
        ),
        Err(OracleError::InvalidFundingAccount.into())
    );

    funding_account.is_writable = true;
    mapping_account.is_writable = false;

    assert_eq!(
        process_instruction(
            &program_id,
            &[funding_account.clone(), mapping_account.clone()],
            instruction_data
        ),
        Err(OracleError::InvalidSignableAccount.into())
    );

    mapping_account.is_writable = true;
    mapping_account.owner = &program_id_2;

    assert_eq!(
        process_instruction(
            &program_id,
            &[funding_account.clone(), mapping_account.clone()],
            instruction_data
        ),
        Err(OracleError::InvalidSignableAccount.into())
    );

    mapping_account.owner = &program_id;
    let prev_data = mapping_account.data;
    mapping_account.data = Rc::new(RefCell::new(&mut []));

    assert_eq!(
        process_instruction(
            &program_id,
            &[funding_account.clone(), mapping_account.clone()],
            instruction_data
        ),
        Err(OracleError::InvalidSignableAccount.into())
    );

    mapping_account.data = prev_data;

    assert!(process_instruction(
        &program_id,
        &[funding_account.clone(), mapping_account.clone()],
        instruction_data
    )
    .is_ok());
}
