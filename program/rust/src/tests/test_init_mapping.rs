use crate::c_oracle_header::{
    cmd_hdr_t,
    command_t_e_cmd_init_mapping,
    pc_map_table_t,
    PC_ACCTYPE_MAPPING,
    PC_MAGIC,
    PC_VERSION,
};
use crate::deserialize::load_account_as;
use crate::rust_oracle::{
    clear_account,
    init_mapping,
};
use bytemuck::bytes_of;
use solana_program::account_info::AccountInfo;
use solana_program::clock::Epoch;
use solana_program::native_token::LAMPORTS_PER_SOL;
use solana_program::program_error::ProgramError;
use solana_program::pubkey::Pubkey;
use solana_program::rent::Rent;
use solana_program::system_program;
use std::cell::RefCell;
use std::mem::size_of;
use std::rc::Rc;

#[test]
fn test_init_mapping() {
    let hdr: cmd_hdr_t = cmd_hdr_t {
        ver_: PC_VERSION,
        cmd_: command_t_e_cmd_init_mapping as i32,
    };
    let instruction_data = bytes_of::<cmd_hdr_t>(&hdr);

    let program_id = Pubkey::new_unique();
    let program_id_2 = Pubkey::new_unique();
    let funding_key = Pubkey::new_unique();
    let mapping_key = Pubkey::new_unique();
    let system_program = system_program::id();

    let mut funding_balance = LAMPORTS_PER_SOL.clone();
    let mut funding_account = AccountInfo::new(
        &funding_key,
        true,
        true,
        &mut funding_balance,
        &mut [],
        &system_program,
        false,
        Epoch::default(),
    );

    let mut mapping_balance = Rent::minimum_balance(&Rent::default(), size_of::<pc_map_table_t>());
    let mut mapping_raw_data = [0u8; size_of::<pc_map_table_t>()];

    let mut mapping_account = AccountInfo::new(
        &mapping_key,
        true,
        true,
        &mut mapping_balance,
        &mut mapping_raw_data,
        &program_id,
        false,
        Epoch::default(),
    );

    assert!(init_mapping(
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
        init_mapping(
            &program_id,
            &[funding_account.clone(), mapping_account.clone()],
            instruction_data
        ),
        Err(ProgramError::InvalidArgument)
    );

    clear_account(&mapping_account).unwrap();

    assert_eq!(
        init_mapping(&program_id, &[funding_account.clone()], instruction_data),
        Err(ProgramError::InvalidArgument)
    );

    funding_account.is_signer = false;

    assert_eq!(
        init_mapping(
            &program_id,
            &[funding_account.clone(), mapping_account.clone()],
            instruction_data
        ),
        Err(ProgramError::InvalidArgument)
    );

    funding_account.is_signer = true;
    mapping_account.is_signer = false;

    assert_eq!(
        init_mapping(
            &program_id,
            &[funding_account.clone(), mapping_account.clone()],
            instruction_data
        ),
        Err(ProgramError::InvalidArgument)
    );

    mapping_account.is_signer = true;
    funding_account.is_writable = false;

    assert_eq!(
        init_mapping(
            &program_id,
            &[funding_account.clone(), mapping_account.clone()],
            instruction_data
        ),
        Err(ProgramError::InvalidArgument)
    );

    funding_account.is_writable = true;
    mapping_account.is_writable = false;

    assert_eq!(
        init_mapping(
            &program_id,
            &[funding_account.clone(), mapping_account.clone()],
            instruction_data
        ),
        Err(ProgramError::InvalidArgument)
    );

    mapping_account.is_writable = true;
    mapping_account.owner = &program_id_2;

    assert_eq!(
        init_mapping(
            &program_id,
            &[funding_account.clone(), mapping_account.clone()],
            instruction_data
        ),
        Err(ProgramError::InvalidArgument)
    );

    mapping_account.owner = &program_id;
    let prev_data = mapping_account.data;
    mapping_account.data = Rc::new(RefCell::new(&mut []));

    assert_eq!(
        init_mapping(
            &program_id,
            &[funding_account.clone(), mapping_account.clone()],
            instruction_data
        ),
        Err(ProgramError::InvalidArgument)
    );

    mapping_account.data = prev_data;

    assert!(init_mapping(
        &program_id,
        &[funding_account.clone(), mapping_account.clone()],
        instruction_data
    )
    .is_ok());
}
