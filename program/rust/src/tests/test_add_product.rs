use std::mem::size_of;

use bytemuck::{
    bytes_of,
    Zeroable,
};
use solana_program::account_info::AccountInfo;
use solana_program::clock::Epoch;
use solana_program::native_token::LAMPORTS_PER_SOL;
use solana_program::pubkey::Pubkey;
use solana_program::rent::Rent;
use solana_program::system_program;

use crate::c_oracle_header::{
    cmd_hdr_t,
    command_t_e_cmd_add_product,
    pc_map_table_t,
    pc_prod_t,
    PC_ACCTYPE_MAPPING,
    PC_ACCTYPE_PRODUCT,
    PC_MAGIC,
    PC_MAP_TABLE_SIZE,
    PC_PROD_ACC_SIZE,
    PC_VERSION,
};
use crate::deserialize::load_account_as;
use crate::rust_oracle::{
    add_product,
    clear_account,
    initialize_mapping_account,
    load_mapping_account_mut,
    pubkey_equal,
};

#[test]
fn test_add_product() {
    let hdr = cmd_hdr_t {
        ver_: PC_VERSION,
        cmd_: command_t_e_cmd_add_product as i32,
    };
    let instruction_data = bytes_of::<cmd_hdr_t>(&hdr);

    let program_id = Pubkey::new_unique();
    let funding_key = Pubkey::new_unique();
    let mkey = Pubkey::new_unique();
    let product_key_1 = Pubkey::new_unique();
    let product_key_2 = Pubkey::new_unique();

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

    let mut mapping_balance = Rent::minimum_balance(&Rent::default(), size_of::<pc_map_table_t>());
    let mut mapping_data: pc_map_table_t = pc_map_table_t::zeroed();
    mapping_data.magic_ = PC_MAGIC;
    mapping_data.ver_ = PC_VERSION;
    mapping_data.type_ = PC_ACCTYPE_MAPPING;
    let mut mapping_bytes = bytemuck::bytes_of_mut(&mut mapping_data);

    let mapping_account = AccountInfo::new(
        &mkey,
        true,
        true,
        &mut mapping_balance,
        &mut mapping_bytes,
        &program_id,
        false,
        Epoch::default(),
    );

    let mut product_balance = Rent::minimum_balance(&Rent::default(), PC_PROD_ACC_SIZE as usize);
    let mut prod_raw_data = [0u8; PC_PROD_ACC_SIZE as usize];
    let product_account = AccountInfo::new(
        &product_key_1,
        true,
        true,
        &mut product_balance,
        &mut prod_raw_data,
        &program_id,
        false,
        Epoch::default(),
    );

    let mut product_balance_2 = Rent::minimum_balance(&Rent::default(), PC_PROD_ACC_SIZE as usize);
    let mut prod_raw_data_2 = [0u8; PC_PROD_ACC_SIZE as usize];
    let product_account_2 = AccountInfo::new(
        &product_key_2,
        true,
        true,
        &mut product_balance_2,
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
        let mapping_data = load_mapping_account_mut(&mapping_account, PC_VERSION).unwrap();

        assert_eq!(product_data.magic_, PC_MAGIC);
        assert_eq!(product_data.ver_, PC_VERSION);
        assert_eq!(product_data.type_, PC_ACCTYPE_PRODUCT);
        assert_eq!(product_data.size_, size_of::<pc_prod_t>() as u32);
        assert_eq!(mapping_data.num_, 1);
        assert!(pubkey_equal(
            &mapping_data.prod_[0],
            &product_account.key.to_bytes()
        ));
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
        let mapping_data = load_mapping_account_mut(&mapping_account, PC_VERSION).unwrap();
        assert_eq!(mapping_data.num_, 2);
        assert!(pubkey_equal(
            &mapping_data.prod_[1],
            &product_account_2.key.to_bytes()
        ));
    }

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
    assert!(add_product(
        &program_id,
        &[
            funding_account.clone(),
            mapping_account.clone(),
            product_account_3.clone()
        ],
        instruction_data
    )
    .is_err());

    // test fill up of mapping table
    clear_account(&mapping_account).unwrap();
    initialize_mapping_account(&mapping_account, PC_VERSION).unwrap();

    for i in 0..PC_MAP_TABLE_SIZE {
        clear_account(&product_account).unwrap();

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
        let mapping_data = load_mapping_account_mut(&mapping_account, PC_VERSION).unwrap();
        assert_eq!(mapping_data.num_, i + 1);
    }

    clear_account(&product_account).unwrap();

    assert!(add_product(
        &program_id,
        &[
            funding_account.clone(),
            mapping_account.clone(),
            product_account.clone()
        ],
        instruction_data
    )
    .is_err());

    let mapping_data = load_mapping_account_mut(&mapping_account, PC_VERSION).unwrap();
    assert_eq!(mapping_data.num_, PC_MAP_TABLE_SIZE);
}
