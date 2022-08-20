
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
use crate::processor::process_instruction;
use solana_sdk::signature::{Keypair, Signer};
use solana_sdk::transaction::Transaction;

use crate::c_oracle_header::{cmd_hdr, command_t_e_cmd_del_price, pc_map_table_t, pc_price_t, pc_prod_t, PC_VERSION};

use std::str::FromStr;
use solana_program::account_info::Account;
use solana_program::hash::Hash;
use solana_program::pubkey::Pubkey;
use solana_program_test::{BanksClient, processor, ProgramTest};
use solana_sdk::signature::Keypair;
use solana_sdk::transaction::Transaction;
use crate::c_oracle_header::pc_map_table_t;

struct PythSimulator {
  program_id: Pubkey,
  banks_client: BanksClient,
  payer: Keypair,
  recent_blockhash: Hash
}

impl PythSimulator {
  pub async fn new() -> PythSimulator {
    let program_id = Pubkey::new_unique();
    let (banks_client, payer, recent_blockhash) = ProgramTest::new(
      "pyth",
      program_id,
      processor!(process_instruction),
    )
      .start()
      .await;

    PythSimulator {
      program_id,
      banks_client,
      payer,
      recent_blockhash
    }
  }

  pub async fn init_mapping(&mut self) -> Pubkey {
    let mapping_keypair = Keypair::new();
    let size = size_of::<pc_map_table_t>();
    let rent = Rent::minimum_balance(&Rent::default(), size);
    let instruction = system_instruction::create_account(&self.payer.pubkey(), &mapping_keypair.pubkey(), rent, size as u64, &self.program_id);
    let mut transaction = Transaction::new_with_payer(
      &[instruction],
      Some(&self.payer.pubkey()),
    );
    transaction.sign(&[&self.payer, &mapping_keypair], self.recent_blockhash);
    // transaction.sign(&[&mapping_keypair], recent_blockhash);

    self.banks_client.process_transaction(transaction).await.unwrap();

    mapping_keypair.pubkey()
  }

  pub async fn get_account(&self, key: Pubkey) -> Option<Account> {
    self.banks_client.get_account(mapping_pubkey).await.unwrap()
  }
}