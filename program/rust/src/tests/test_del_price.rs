use bytemuck::bytes_of;
use solana_program::program_error::ProgramError;
use solana_program::pubkey::Pubkey;
use solana_sdk::signer::Signer;

use crate::c_oracle_header::{
    cmd_hdr,
    command_t_e_cmd_del_price,
    pc_price_t,
    pc_prod_t,
    PC_VERSION,
};
use crate::deserialize::{
    initialize_pyth_account_checked,
    load_checked,
};
use crate::rust_oracle::del_price;
use crate::tests::test_tx_utils::PythSimulator;
use crate::tests::test_utils::AccountSetup;
use crate::utils::{
    pubkey_assign,
    pubkey_is_zero,
};

#[test]
fn test_del_price() {
    let program_id = Pubkey::new_unique();
    let hdr = del_price_instruction();
    let instruction_data = bytes_of(&hdr);

    let mut funding_setup = AccountSetup::new_funding();
    let funding_account = funding_setup.to_account_info();

    let mut product_setup = AccountSetup::new::<pc_prod_t>(&program_id);
    let product_account = product_setup.to_account_info();
    initialize_pyth_account_checked::<pc_prod_t>(&product_account, PC_VERSION).unwrap();

    let mut price_setup = AccountSetup::new::<pc_price_t>(&program_id);
    let price_account = price_setup.to_account_info();
    initialize_pyth_account_checked::<pc_price_t>(&price_account, PC_VERSION).unwrap();

    let mut system_setup = AccountSetup::new_system_program();
    let system_account = system_setup.to_account_info();

    // Try deleting a price account that isn't linked to the given product account
    assert_eq!(
        del_price(
            &program_id,
            &[
                funding_account.clone(),
                product_account.clone(),
                price_account.clone(),
                system_account.clone()
            ],
            &instruction_data
        ),
        Err(ProgramError::InvalidArgument)
    );

    // Same test with a random nonzero pubkey
    {
        let mut product_data = load_checked::<pc_prod_t>(&product_account, PC_VERSION).unwrap();
        pubkey_assign(&mut product_data.px_acc_, &Pubkey::new_unique().to_bytes());
    }
    assert_eq!(
        del_price(
            &program_id,
            &[
                funding_account.clone(),
                product_account.clone(),
                price_account.clone(),
                system_account.clone()
            ],
            &instruction_data
        ),
        Err(ProgramError::InvalidArgument)
    );

    // Note that we can't test success outside of the solana vm because of the system program.
}

#[tokio::test]
async fn test_del_price_integration() {
    let mut sim = PythSimulator::new().await;
    let mapping_keypair = sim.init_mapping().await.unwrap();
    let product1 = sim.add_product(&mapping_keypair).await.unwrap();
    let product2 = sim.add_product(&mapping_keypair).await.unwrap();
    let product3 = sim.add_product(&mapping_keypair).await.unwrap();
    let price1 = sim.add_price(&product1, -8).await.unwrap();
    let price2_1 = sim.add_price(&product2, -8).await.unwrap();
    let price2_2 = sim.add_price(&product2, -8).await.unwrap();

    assert!(sim.get_account(price1.pubkey()).await.is_some());
    assert!(sim.get_account(price2_1.pubkey()).await.is_some());

    assert!(sim.del_price(&product2, &price1).await.is_err());
    assert!(sim.del_price(&product2, &price2_1).await.is_err());
    assert!(sim.del_price(&product1, &price2_2).await.is_err());
    assert!(sim.del_price(&product3, &price2_2).await.is_err());

    sim.del_price(&product1, &price1).await.unwrap();
    assert!(sim.get_account(price1.pubkey()).await.is_none());

    let product1_data = sim
        .get_account_data_as::<pc_prod_t>(product1.pubkey())
        .await
        .unwrap();
    assert!(pubkey_is_zero(&product1_data.px_acc_));
}

fn del_price_instruction() -> cmd_hdr {
    cmd_hdr {
        ver_: PC_VERSION,
        cmd_: command_t_e_cmd_del_price as i32,
    }
}
