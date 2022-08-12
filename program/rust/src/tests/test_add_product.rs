use std::mem::size_of;

use crate::error::OracleError;
use crate::tests::test_utils::AccountSetup;
use bytemuck::bytes_of;
use solana_program::account_info::AccountInfo;
use solana_program::clock::Epoch;
use solana_program::program_error::ProgramError;
use solana_program::pubkey::Pubkey;
use solana_program::rent::Rent;

use crate::c_oracle_header::{
    cmd_hdr_t,
    command_t_e_cmd_add_product,
    pc_map_table_t,
    pc_prod_t,
    PythAccount,
    PC_ACCTYPE_PRODUCT,
    PC_MAGIC,
    PC_MAP_TABLE_SIZE,
    PC_PROD_ACC_SIZE,
    PC_VERSION,
};
use crate::deserialize::{
    initialize_pyth_account_checked,
    load_account_as,
    load_checked,
};
use crate::utils::{
    clear_account,
    pubkey_equal,
};

use crate::rust_oracle::add_product;

#[test]
fn test_add_product() {
    let hdr = cmd_hdr_t {
        ver_: PC_VERSION,
        cmd_: command_t_e_cmd_add_product as i32,
    };
    let instruction_data = bytes_of::<cmd_hdr_t>(&hdr);

    let program_id = Pubkey::new_unique();

    let mut funding_setup = AccountSetup::new_funding();
    let funding_account = funding_setup.to_account_info();

    let mut mapping_setup = AccountSetup::new::<pc_map_table_t>(&program_id);
    let mapping_account = mapping_setup.to_account_info();
    initialize_pyth_account_checked::<pc_map_table_t>(&mapping_account, PC_VERSION).unwrap();

    let mut product_setup = AccountSetup::new::<pc_prod_t>(&program_id);
    let product_account = product_setup.to_account_info();

    let mut product_setup_2 = AccountSetup::new::<pc_prod_t>(&program_id);
    let product_account_2 = product_setup_2.to_account_info();

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
        let mapping_data = load_checked::<pc_map_table_t>(&mapping_account, PC_VERSION).unwrap();

        assert_eq!(product_data.magic_, PC_MAGIC);
        assert_eq!(product_data.ver_, PC_VERSION);
        assert_eq!(product_data.type_, PC_ACCTYPE_PRODUCT);
        assert_eq!(product_data.size_, size_of::<pc_prod_t>() as u32);
        assert_eq!(mapping_data.num_, 1);
        assert_eq!(mapping_data.size_, (pc_map_table_t::INITIAL_SIZE + 32));
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
        let mapping_data = load_checked::<pc_map_table_t>(&mapping_account, PC_VERSION).unwrap();
        assert_eq!(mapping_data.num_, 2);
        assert_eq!(mapping_data.size_, (pc_map_table_t::INITIAL_SIZE + 2 * 32));
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
    assert_eq!(
        add_product(
            &program_id,
            &[
                funding_account.clone(),
                mapping_account.clone(),
                product_account_3.clone()
            ],
            instruction_data
        ),
        Err(OracleError::InvalidSignableAccount.into())
    );

    // test fill up of mapping table
    clear_account(&mapping_account).unwrap();
    initialize_pyth_account_checked::<pc_map_table_t>(&mapping_account, PC_VERSION).unwrap();

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
        let mapping_data = load_checked::<pc_map_table_t>(&mapping_account, PC_VERSION).unwrap();
        assert_eq!(
            mapping_data.size_,
            pc_map_table_t::INITIAL_SIZE + (i + 1) * 32
        );
        assert_eq!(mapping_data.num_, i + 1);
    }

    clear_account(&product_account).unwrap();

    assert_eq!(
        add_product(
            &program_id,
            &[
                funding_account.clone(),
                mapping_account.clone(),
                product_account.clone()
            ],
            instruction_data
        ),
        Err(ProgramError::InvalidArgument)
    );

    let mapping_data = load_checked::<pc_map_table_t>(&mapping_account, PC_VERSION).unwrap();
    assert_eq!(mapping_data.num_, PC_MAP_TABLE_SIZE);
}
