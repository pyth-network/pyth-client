use crate::c_oracle_header::{
    cmd_hdr_t,
    command_t_e_cmd_add_mapping,
    pc_map_table_t,
    PC_MAGIC,
    PC_MAP_TABLE_SIZE,
    PC_VERSION,
};
use crate::deserialize::load_account_as_mut;
use crate::rust_oracle::{
    add_mapping,
    clear_account,
    initialize_checked,
    load_checked,
    pubkey_assign,
    pubkey_equal,
    pubkey_is_zero,
};
use crate::tests::test_utils::AccountSetup;
use bytemuck::bytes_of;
use solana_program::program_error::ProgramError;
use solana_program::pubkey::Pubkey;

#[test]
fn test_add_mapping() {
    let hdr: cmd_hdr_t = cmd_hdr_t {
        ver_: PC_VERSION,
        cmd_: command_t_e_cmd_add_mapping as i32,
    };
    let instruction_data = bytes_of::<cmd_hdr_t>(&hdr);

    let program_id = Pubkey::new_unique();

    let mut funding_setup = AccountSetup::new_funding();
    let funding_account = funding_setup.to_account_info();

    let mut curr_mapping_setup = AccountSetup::new::<pc_map_table_t>(&program_id);
    let cur_mapping = curr_mapping_setup.to_account_info();
    initialize_checked::<pc_map_table_t>(&cur_mapping, PC_VERSION).unwrap();

    let mut next_mapping_setup = AccountSetup::new::<pc_map_table_t>(&program_id);
    let next_mapping = next_mapping_setup.to_account_info();

    {
        let mut cur_mapping_data =
            load_checked::<pc_map_table_t>(&cur_mapping, PC_VERSION).unwrap();
        cur_mapping_data.num_ = PC_MAP_TABLE_SIZE;
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

    {
        let next_mapping_data = load_checked::<pc_map_table_t>(&next_mapping, PC_VERSION).unwrap();
        let mut cur_mapping_data =
            load_checked::<pc_map_table_t>(&cur_mapping, PC_VERSION).unwrap();

        assert!(pubkey_equal(
            &cur_mapping_data.next_,
            &next_mapping.key.to_bytes()
        ));
        assert!(pubkey_is_zero(&next_mapping_data.next_));
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
        let mut cur_mapping_data =
            load_checked::<pc_map_table_t>(&cur_mapping, PC_VERSION).unwrap();
        assert!(pubkey_is_zero(&cur_mapping_data.next_));
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
