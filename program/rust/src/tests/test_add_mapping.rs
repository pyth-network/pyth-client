mod test {
    use crate::c_oracle_header::{
        cmd_hdr_t,
        command_t_e_cmd_add_mapping,
        pc_map_table_t,
        PC_MAP_TABLE_SIZE,
        PC_VERSION,
    };
    use crate::rust_oracle::{
        add_mapping,
        initialize_mapping_account,
        load_mapping_account_mut,
    };
    use bytemuck::bytes_of;
    use solana_program::account_info::AccountInfo;
    use solana_program::clock::Epoch;
    use solana_program::native_token::LAMPORTS_PER_SOL;
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
    }
}
