use crate::c_oracle_header::{
    cmd_hdr_t,
    command_t_e_cmd_add_mapping,
    pc_map_table_t,
    PC_MAGIC,
    PC_MAP_TABLE_SIZE,
    PC_VERSION,
};
use crate::rust_oracle::{
    add_mapping,
    clear_account,
    initialize_mapping_account,
    load_account_as_mut,
    load_mapping_account_mut,
    pubkey_assign,
};
use bytemuck::bytes_of;
use solana_program::account_info::AccountInfo;
use solana_program::clock::Epoch;
use solana_program::native_token::LAMPORTS_PER_SOL;
use solana_program::program_error::ProgramError;
use solana_program::pubkey::Pubkey;
use solana_program::rent::Rent;
use solana_program::system_program;
use std::mem::size_of;

#[test]
fn test_add_mapping() {
    let hdr: cmd_hdr_t = cmd_hdr_t {
        ver_: PC_VERSION,
        cmd_: command_t_e_cmd_add_mapping as i32,
    };
    let instruction_data = bytes_of::<cmd_hdr_t>(&hdr);

    let program_id = Pubkey::new_unique();
    let funding_key = Pubkey::new_unique();
    let cur_mapping_key = Pubkey::new_unique();
    let next_mapping_key = Pubkey::new_unique();
    let system_program = system_program::id();

    let mut funding_balance = LAMPORTS_PER_SOL.clone();
    let funding_account = AccountInfo::new(
        &funding_key,
        true,
        true,
        &mut funding_balance,
        &mut [],
        &system_program,
        false,
        Epoch::default(),
    );

    let mut cur_mapping_balance =
        Rent::minimum_balance(&Rent::default(), size_of::<pc_map_table_t>());
    let mut cur_mapping_raw_data = [0u8; size_of::<pc_map_table_t>()];

    let cur_mapping = AccountInfo::new(
        &cur_mapping_key,
        true,
        true,
        &mut cur_mapping_balance,
        &mut cur_mapping_raw_data,
        &program_id,
        false,
        Epoch::default(),
    );

    initialize_mapping_account(&cur_mapping, PC_VERSION).unwrap();

    {
        let mut cur_mapping_data = load_mapping_account_mut(&cur_mapping, PC_VERSION).unwrap();
        cur_mapping_data.num_ = PC_MAP_TABLE_SIZE;
    }

    let mut next_mapping_balance =
        Rent::minimum_balance(&Rent::default(), size_of::<pc_map_table_t>());
    let mut next_mapping_raw_data = [0u8; size_of::<pc_map_table_t>()];

    let next_mapping = AccountInfo::new(
        &next_mapping_key,
        true,
        true,
        &mut next_mapping_balance,
        &mut next_mapping_raw_data,
        &program_id,
        false,
        Epoch::default(),
    );

    assert!(add_mapping(
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
        let next_mapping_data = load_mapping_account_mut(&next_mapping, PC_VERSION).unwrap();
        let mut cur_mapping_data = load_mapping_account_mut(&cur_mapping, PC_VERSION).unwrap();

        assert!(unsafe {
            cur_mapping_data
                .next_
                .k1_
                .iter()
                .zip(&next_mapping_key.to_bytes())
                .all(|(x, y)| *x == *y)
        });
        assert!(unsafe { next_mapping_data.next_.k8_.iter().all(|x| *x == 0) });
        pubkey_assign(&mut cur_mapping_data.next_, &Pubkey::default().to_bytes());
        cur_mapping_data.num_ = 0;
    }

    clear_account(&next_mapping).unwrap();

    assert_eq!(
        add_mapping(
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
        let mut cur_mapping_data = load_mapping_account_mut(&cur_mapping, PC_VERSION).unwrap();
        assert!(unsafe { cur_mapping_data.next_.k8_.iter().all(|x| *x == 0) });
        cur_mapping_data.num_ = PC_MAP_TABLE_SIZE;
        cur_mapping_data.magic_ = 0;
    }

    assert_eq!(
        add_mapping(
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
        let mut cur_mapping_data = load_account_as_mut::<pc_map_table_t>(&cur_mapping).unwrap();
        cur_mapping_data.magic_ = PC_MAGIC;
    }

    assert!(add_mapping(
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
