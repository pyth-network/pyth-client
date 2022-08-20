use std::mem::size_of;
use std::str::FromStr;

use bytemuck::bytes_of;
use solana_program::system_instruction;
use solana_program::program_error::ProgramError;
use solana_program::pubkey::Pubkey;
use solana_program::rent::Rent;
use solana_program_test::{
    processor,
    ProgramTest,
};
use solana_sdk::signature::{Keypair, Signer};
use solana_sdk::transaction::Transaction;

use crate::c_oracle_header::{cmd_hdr, command_t_e_cmd_del_price, pc_map_table_t, pc_price_t, pc_prod_t, PC_VERSION};
use crate::deserialize::{
    initialize_pyth_account_checked,
    load_checked,
};
use crate::processor::process_instruction;
use crate::rust_oracle::del_price;
use crate::tests::test_utils::AccountSetup;
use crate::utils::pubkey_assign;

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
async fn test_sysvar() {

    /*
    let program_id = Pubkey::from_str("Pyth111111111111111111111111111111111111111").unwrap();
    let (mut banks_client, payer, recent_blockhash) = ProgramTest::new(
        "pyth",
        program_id,
        processor!(process_instruction),
    )
    .start()
    .await;

    let hdr = del_price_instruction();
    let _instruction_data = bytes_of(&hdr);

*/

    let sim = PythSimulator::new();
    let mapping_pubkey = sim.init_mapping();

    let foo = banks_client.get_account(mapping_pubkey).await.unwrap().unwrap();

    assert_eq!(foo.data.len(), 7);

    /*
    let mut transaction = Transaction::new_with_payer(
        &[Instruction::new_with_bincode(
            program_id,
            &(),
            vec![
                AccountMeta::new(sysvar::clock::id(), false),
                AccountMeta::new(sysvar::rent::id(), false),
            ],
        )],
        Some(&payer.pubkey()),
    );
    transaction.sign(&[&payer], recent_blockhash);
    banks_client.process_transaction(transaction).await.unwrap();
     */
}

fn del_price_instruction() -> cmd_hdr {
    cmd_hdr {
        ver_: PC_VERSION,
        cmd_: command_t_e_cmd_del_price as i32,
    }
}