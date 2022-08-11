use crate::error::OracleError;
use crate::tests::test_utils::AccountSetup;
use bytemuck::bytes_of;
use solana_program::program_error::ProgramError;
use solana_program::pubkey::Pubkey;

use crate::c_oracle_header::{
    cmd_add_price,
    cmd_hdr_t,
    command_t_e_cmd_add_price,
    command_t_e_cmd_add_product,
    pc_map_table_t,
    pc_price_t,
    pc_prod_t,
    PC_VERSION,
};
use crate::rust_oracle::{
    add_price,
    add_product,
    clear_account,
    initialize_checked,
    load_checked,
    pubkey_equal,
    pubkey_is_zero,
};

#[test]
fn test_add_price() {
    let hdr_add_product = cmd_hdr_t {
        ver_: PC_VERSION,
        cmd_: command_t_e_cmd_add_product as i32,
    };

    let mut hdr_add_price = cmd_add_price {
        ver_:   PC_VERSION,
        cmd_:   command_t_e_cmd_add_price as i32,
        expo_:  1,
        ptype_: 1,
    };
    let instruction_data_add_product = bytes_of::<cmd_hdr_t>(&hdr_add_product);
    let mut instruction_data_add_price = bytes_of::<cmd_add_price>(&hdr_add_price);

    let program_id = Pubkey::new_unique();

    let mut funding_setup = AccountSetup::new_funding();
    let funding_account = funding_setup.to_account_info();

    let mut mapping_setup = AccountSetup::new::<pc_map_table_t>(&program_id);
    let mapping_account = mapping_setup.to_account_info();
    initialize_checked::<pc_map_table_t>(&mapping_account, PC_VERSION).unwrap();

    let mut product_setup = AccountSetup::new::<pc_prod_t>(&program_id);
    let product_account = product_setup.to_account_info();

    let mut price_setup = AccountSetup::new::<pc_price_t>(&program_id);
    let mut price_account = price_setup.to_account_info();

    let mut price_setup_2 = AccountSetup::new::<pc_price_t>(&program_id);
    let price_account_2 = price_setup_2.to_account_info();

    assert!(add_product(
        &program_id,
        &[
            funding_account.clone(),
            mapping_account.clone(),
            product_account.clone()
        ],
        instruction_data_add_product
    )
    .is_ok());

    assert!(add_price(
        &program_id,
        &[
            funding_account.clone(),
            product_account.clone(),
            price_account.clone()
        ],
        instruction_data_add_price
    )
    .is_ok());

    {
        let price_data = load_checked::<pc_price_t>(&price_account, PC_VERSION).unwrap();
        let product_data = load_checked::<pc_prod_t>(&product_account, PC_VERSION).unwrap();
        assert_eq!(price_data.expo_, 1);
        assert_eq!(price_data.ptype_, 1);
        assert!(pubkey_equal(
            &price_data.prod_,
            &product_account.key.to_bytes()
        ));
        assert!(pubkey_is_zero(&price_data.next_));
        assert!(pubkey_equal(
            &product_data.px_acc_,
            &price_account.key.to_bytes()
        ));
    }

    assert!(add_price(
        &program_id,
        &[
            funding_account.clone(),
            product_account.clone(),
            price_account_2.clone()
        ],
        instruction_data_add_price
    )
    .is_ok());

    {
        let price_data_2 = load_checked::<pc_price_t>(&price_account_2, PC_VERSION).unwrap();
        let product_data = load_checked::<pc_prod_t>(&product_account, PC_VERSION).unwrap();
        assert_eq!(price_data_2.expo_, 1);
        assert_eq!(price_data_2.ptype_, 1);
        assert!(pubkey_equal(
            &price_data_2.prod_,
            &product_account.key.to_bytes()
        ));
        assert!(pubkey_equal(
            &price_data_2.next_,
            &price_account.key.to_bytes()
        ));
        assert!(pubkey_equal(
            &product_data.px_acc_,
            &price_account_2.key.to_bytes()
        ));
    }

    // Wrong number of accounts
    assert_eq!(
        add_price(
            &program_id,
            &[funding_account.clone(), product_account.clone()],
            instruction_data_add_price
        ),
        Err(ProgramError::InvalidArgument)
    );

    // Price account is already initialized
    assert_eq!(
        add_price(
            &program_id,
            &[
                funding_account.clone(),
                product_account.clone(),
                price_account.clone()
            ],
            instruction_data_add_price
        ),
        Err(OracleError::InvalidFreshAccount.into())
    );

    clear_account(&price_account).unwrap();

    // Wrong ptype
    hdr_add_price = cmd_add_price {
        ver_:   PC_VERSION,
        cmd_:   command_t_e_cmd_add_price as i32,
        expo_:  6,
        ptype_: 0,
    };
    instruction_data_add_price = bytes_of::<cmd_add_price>(&hdr_add_price);


    assert_eq!(
        add_price(
            &program_id,
            &[
                funding_account.clone(),
                product_account.clone(),
                price_account.clone()
            ],
            instruction_data_add_price
        ),
        Err(ProgramError::InvalidArgument)
    );


    //Price not signing
    hdr_add_price = cmd_add_price {
        ver_:   PC_VERSION,
        cmd_:   command_t_e_cmd_add_price as i32,
        expo_:  6,
        ptype_: 1,
    };
    instruction_data_add_price = bytes_of::<cmd_add_price>(&hdr_add_price);
    price_account.is_signer = false;

    assert_eq!(
        add_price(
            &program_id,
            &[
                funding_account.clone(),
                product_account.clone(),
                price_account.clone()
            ],
            instruction_data_add_price
        ),
        Err(OracleError::InvalidSignableAccount.into())
    );

    // Fresh product account
    price_account.is_signer = true;
    clear_account(&product_account).unwrap();


    assert_eq!(
        add_price(
            &program_id,
            &[
                funding_account.clone(),
                product_account.clone(),
                price_account.clone()
            ],
            instruction_data_add_price
        ),
        Err(ProgramError::InvalidArgument)
    );
}
