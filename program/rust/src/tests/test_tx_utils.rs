use std::mem::size_of;

use bytemuck::bytes_of;
use solana_program::hash::Hash;
use solana_program::instruction::{
    AccountMeta,
    Instruction,
};
use solana_program::pubkey::Pubkey;
use solana_program::rent::Rent;
use solana_program::{system_instruction, system_program};
use solana_program_test::{
    BanksClient,
    processor,
    ProgramTest,
};
use solana_sdk::account::Account;
use solana_sdk::signature::{
    Keypair,
    Signer,
};
use solana_sdk::transaction::Transaction;

use crate::c_oracle_header::{cmd_add_price_t, cmd_hdr_t, command_t_e_cmd_add_price, command_t_e_cmd_add_product, command_t_e_cmd_del_price, command_t_e_cmd_init_mapping, pc_map_table_t, pc_price_t, PC_PROD_ACC_SIZE, PC_PTYPE_PRICE, PC_VERSION};
use crate::processor::process_instruction;

pub struct PythSimulator {
    program_id:       Pubkey,
    banks_client:     BanksClient,
    payer:            Keypair,
    recent_blockhash: Hash,
}

impl PythSimulator {
    pub async fn new() -> PythSimulator {
        let program_id = Pubkey::new_unique();
        let (banks_client, payer, recent_blockhash) =
            ProgramTest::new("pyth", program_id, processor!(process_instruction))
                .start()
                .await;

        PythSimulator {
            program_id,
            banks_client,
            payer,
            recent_blockhash,
        }
    }

    /// Create an account owned by the pyth program containing `size` bytes.
    /// The account will be created with enough lamports to be rent-exempt.
    pub async fn create_pyth_account(&mut self, size: usize) -> Keypair {
        let keypair = Keypair::new();
        let rent = Rent::minimum_balance(&Rent::default(), size);
        let instruction = system_instruction::create_account(
            &self.payer.pubkey(),
            &keypair.pubkey(),
            rent,
            size as u64,
            &self.program_id,
        );
        let mut transaction =
            Transaction::new_with_payer(&[instruction], Some(&self.payer.pubkey()));
        transaction.sign(&[&self.payer, &keypair], self.recent_blockhash);
        self.banks_client
            .process_transaction(transaction)
            .await
            .unwrap();

        keypair
    }

    pub async fn init_mapping(&mut self) -> Keypair {
        let mapping_keypair = self.create_pyth_account(size_of::<pc_map_table_t>()).await;

        let cmd = cmd_hdr_t {
            ver_: PC_VERSION,
            cmd_: command_t_e_cmd_init_mapping as i32,
        };
        let instruction = Instruction::new_with_bytes(
            self.program_id,
            bytes_of(&cmd),
            vec![
                AccountMeta::new(self.payer.pubkey(), true),
                AccountMeta::new(mapping_keypair.pubkey(), true),
            ],
        );

        let mut transaction =
            Transaction::new_with_payer(&[instruction], Some(&self.payer.pubkey()));
        transaction.sign(&[&self.payer, &mapping_keypair], self.recent_blockhash);
        self.banks_client
            .process_transaction(transaction)
            .await
            .unwrap();

        mapping_keypair
    }

    pub async fn add_product(&mut self, mapping_keypair: &Keypair) -> Keypair {
        let product_keypair = self.create_pyth_account(PC_PROD_ACC_SIZE as usize).await;

        let cmd = cmd_hdr_t {
            ver_: PC_VERSION,
            cmd_: command_t_e_cmd_add_product as i32,
        };
        let instruction = Instruction::new_with_bytes(
            self.program_id,
            bytes_of(&cmd),
            vec![
                AccountMeta::new(self.payer.pubkey(), true),
                AccountMeta::new(mapping_keypair.pubkey(), true),
                AccountMeta::new(product_keypair.pubkey(), true),
            ],
        );

        let mut transaction =
            Transaction::new_with_payer(&[instruction], Some(&self.payer.pubkey()));
        transaction.sign(
            &[&self.payer, &mapping_keypair, &product_keypair],
            self.recent_blockhash,
        );
        self.banks_client
            .process_transaction(transaction)
            .await
            .unwrap();

        product_keypair
    }

    pub async fn add_price(&mut self, product_keypair: &Keypair, expo: i32) -> Keypair {
        let price_keypair = self.create_pyth_account(size_of::<pc_price_t>()).await;

        let cmd = cmd_add_price_t {
            ver_:   PC_VERSION,
            cmd_:   command_t_e_cmd_add_price as i32,
            expo_:  expo,
            ptype_: PC_PTYPE_PRICE,
        };
        let instruction = Instruction::new_with_bytes(
            self.program_id,
            bytes_of(&cmd),
            vec![
                AccountMeta::new(self.payer.pubkey(), true),
                AccountMeta::new(product_keypair.pubkey(), true),
                AccountMeta::new(price_keypair.pubkey(), true),
            ],
        );

        let mut transaction =
            Transaction::new_with_payer(&[instruction], Some(&self.payer.pubkey()));
        transaction.sign(
            &[&self.payer, &product_keypair, &price_keypair],
            self.recent_blockhash,
        );
        self.banks_client
            .process_transaction(transaction)
            .await
            .unwrap();

        price_keypair
    }

    pub async fn del_price(&mut self, product_keypair: &Keypair, price_keypair: &Keypair) -> () {
        let cmd = cmd_hdr_t {
            ver_:   PC_VERSION,
            cmd_:   command_t_e_cmd_del_price as i32,
        };
        let instruction = Instruction::new_with_bytes(
            self.program_id,
            bytes_of(&cmd),
            vec![
                AccountMeta::new(self.payer.pubkey(), true),
                AccountMeta::new(product_keypair.pubkey(), true),
                AccountMeta::new(price_keypair.pubkey(), true),
            ],
        );

        let mut transaction =
          Transaction::new_with_payer(&[instruction], Some(&self.payer.pubkey()));
        transaction.sign(
            &[&self.payer, &product_keypair, &price_keypair],
            self.recent_blockhash,
        );
        self.banks_client
          .process_transaction(transaction)
          .await
          .unwrap();
    }

    pub async fn get_account(&mut self, key: Pubkey) -> Option<Account> {
        self.banks_client.get_account(key).await.unwrap()
    }
}
