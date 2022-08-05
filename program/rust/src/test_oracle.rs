#[cfg(test)]
mod test {
    use crate::c_oracle_header::{
        cmd_hdr_t,
        command_t_e_cmd_add_product,
        command_t_e_cmd_init_mapping,
        pc_map_table_t,
        pc_prod_t,
        PC_ACCTYPE_MAPPING,
        PC_ACCTYPE_PRODUCT,
        PC_MAGIC,
        PC_PROD_ACC_SIZE,
        PC_VERSION,
    };
    use crate::rust_oracle::{
        add_product,
        clear_account,
        init_mapping,
        load_account_as,
        load_mapping_account_mut,
    };
    use bytemuck::{
        bytes_of,
        Zeroable,
    };
    use solana_program::account_info::{
        Account,
        AccountInfo,
    };
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

        let mut mapping_balance =
            Rent::minimum_balance(&Rent::default(), size_of::<pc_map_table_t>());
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

    fn fresh_funding_account<'a>() -> AccountInfo<'a> {
        let system_program = system_program::id();

        let mut funding_balance = LAMPORTS_PER_SOL.clone();
        AccountInfo::new(
            &Pubkey::new_unique(),
            true,
            true,
            &mut funding_balance,
            &mut [],
            &system_program,
            false,
            Epoch::default(),
        )
    }

    /*
    macro_rules! zero_account {
        ($size:expr) => {
            AccountInfo::new(
                &Pubkey::new_unique(),
                true,
                true,
                &mut Rent::minimum_balance(&Rent::default(), size as usize),
                &mut [0u8; size],
                &program_id,
                false,
                Epoch::default(),
            );
        }
    }
     */

    #[test]
    fn test_add_product() {
        let hdr = cmd_hdr_t {
            ver_: PC_VERSION,
            cmd_: command_t_e_cmd_add_product as i32,
        };
        let instruction_data = bytes_of::<cmd_hdr_t>(&hdr);

        let program_id = Pubkey::new_unique();
        let mkey = Pubkey::new_unique();
        let product_key_1 = Pubkey::new_unique();
        let product_key_2 = Pubkey::new_unique();

        let mut funding_account = fresh_funding_account();
        let pkey = funding_account.key;

        let mut mapping_balance =
            Rent::minimum_balance(&Rent::default(), size_of::<pc_map_table_t>());
        let mut mapping_data: pc_map_table_t = pc_map_table_t::zeroed();
        mapping_data.magic_ = PC_MAGIC;
        mapping_data.ver_ = PC_VERSION;
        mapping_data.type_ = PC_ACCTYPE_MAPPING;

        let mapping_account = AccountInfo::new(
            &mkey,
            true,
            true,
            &mut mapping_balance,
            &mut bytemuck::bytes_of_mut(&mut mapping_data),
            &program_id,
            false,
            Epoch::default(),
        );

        let mut prod_raw_data = [0u8; PC_PROD_ACC_SIZE as usize];
        let product_account = AccountInfo::new(
            &product_key_1,
            true,
            true,
            &mut Rent::minimum_balance(&Rent::default(), PC_PROD_ACC_SIZE as usize),
            &mut prod_raw_data,
            &program_id,
            false,
            Epoch::default(),
        );

        let mut prod_raw_data_2 = [0u8; PC_PROD_ACC_SIZE as usize];
        let product_account_2 = AccountInfo::new(
            &product_key_2,
            true,
            true,
            &mut Rent::minimum_balance(&Rent::default(), PC_PROD_ACC_SIZE as usize),
            &mut prod_raw_data_2,
            &program_id,
            false,
            Epoch::default(),
        );

        assert!(add_product(
            &program_id,
            &[
                funding_account.clone(),
                mapping_account.clone(),
                product_account.clone()
            ],
            instruction_data
        )
        .is_ok());

        {
            let product_data = load_account_as::<pc_prod_t>(&product_account).unwrap();
            let mapping_data = load_mapping_account_mut(&mapping_account).unwrap();

            assert_eq!(product_data.magic_, PC_MAGIC);
            assert_eq!(product_data.ver_, PC_VERSION);
            assert_eq!(product_data.type_, PC_ACCTYPE_PRODUCT);
            assert_eq!(product_data.size_, size_of::<pc_prod_t>);
            assert_eq!(mapping_data.num_, 1);
            assert_eq!(mapping_data.prod_[0].k1_, product_account.key().to_bytes());
        }

        assert!(add_product(
            &program_id,
            &[
                funding_account.clone(),
                mapping_account.clone(),
                product_account_2.clone()
            ],
            instruction_data
        )
        .is_ok());
        {
            let mapping_data = load_mapping_account_mut(&mapping_account).unwrap();
            assert_eq!(mapping_data.num_, 2);
            assert_eq!(
                mapping_data.prod_[1].k1_,
                product_account_2.key().to_bytes()
            );
        }

        /*
        // invalid acc size
        acc[2].data_len = 1;
        cr_assert(  ERROR_INVALID_ARGUMENT== dispatch( &prm, acc ) );
        acc[2].data_len = PC_PROD_ACC_SIZE;

        // test fill up of mapping table
        sol_memset( mptr, 0, sizeof( pc_map_table_t ) );
        mptr->magic_ = PC_MAGIC;
        mptr->ver_ = PC_VERSION;
        mptr->type_ = PC_ACCTYPE_MAPPING;
        for( unsigned i = 0;; ++i ) {
            sol_memset( sptr, 0, PC_PROD_ACC_SIZE );
            uint64_t rc = dispatch( &prm, acc );
            if ( rc != SUCCESS ) {
                cr_assert( i == ( unsigned )(PC_MAP_TABLE_SIZE) );
                break;
            }
            cr_assert( mptr->num_ == i + 1 );
            cr_assert( rc == SUCCESS );
        }
         */
    }
}
