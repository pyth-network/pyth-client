use std::mem::size_of;

use bytemuck::{
    bytes_of,
    Pod,
};
use solana_program::bpf_loader_upgradeable::{
    self,
    UpgradeableLoaderState,
};
use solana_program::hash::Hash;
use solana_program::instruction::{
    AccountMeta,
    Instruction,
};
use solana_program::native_token::LAMPORTS_PER_SOL;
use solana_program::pubkey::Pubkey;
use solana_program::rent::Rent;
use solana_program::stake_history::Epoch;
use solana_program::{
    system_instruction,
    system_program,
};
use solana_program_test::{
    find_file,
    read_file,
    BanksClient,
    BanksClientError,
    ProgramTest,
    ProgramTestBanksClientExt,
};
use solana_sdk::account::Account;
use solana_sdk::signature::{
    Keypair,
    Signer,
};
use solana_sdk::transaction::Transaction;

use crate::c_oracle_header::{
    MappingAccount,
    PriceAccount,
    PC_PROD_ACC_SIZE,
    PC_PTYPE_PRICE,
    PERMISSIONS_SEED,
};
use crate::deserialize::load;
use crate::instruction::{
    AddPriceArgs,
    CommandHeader,
    OracleCommand,
    UpdPermissionsArgs,
};


/// Simulator for the state of the pyth program on Solana. You can run solana transactions against
/// this struct to test how pyth instructions execute in the Solana runtime.
pub struct PythSimulator {
    program_id:     Pubkey,
    banks_client:   BanksClient,
    payer:          Keypair,
    /// Hash used to submit the last transaction. The hash must be advanced for each new
    /// transaction; otherwise, replayed transactions in different states can return stale
    /// results.
    last_blockhash: Hash,
    programdata_id: Pubkey,
}

impl PythSimulator {
    /// Deploys the oracle program as upgradable
    pub async fn new() -> PythSimulator {
        let mut bpf_data = read_file(find_file("pyth_oracle.so").unwrap_or_else(|| {
            panic!("Unable to locate {}", "pyth_oracle.so");
        }));


        let mut program_test = ProgramTest::default();
        let program_key = Pubkey::new_unique();
        let programdata_key = Pubkey::new_unique();

        let upgrade_authority_keypair = Keypair::new();

        let program_deserialized = UpgradeableLoaderState::Program {
            programdata_address: programdata_key,
        };
        let programdata_deserialized = UpgradeableLoaderState::ProgramData {
            slot:                      1,
            upgrade_authority_address: Some(upgrade_authority_keypair.pubkey()),
        };

        // Program contains a pointer to progradata
        let program_vec = bincode::serialize(&program_deserialized).unwrap();
        // Programdata contains a header and the binary of the program
        let mut programdata_vec = bincode::serialize(&programdata_deserialized).unwrap();
        programdata_vec.append(&mut bpf_data);

        let program_account = Account {
            lamports:   Rent::default().minimum_balance(program_vec.len()),
            data:       program_vec,
            owner:      bpf_loader_upgradeable::ID,
            executable: true,
            rent_epoch: Epoch::default(),
        };
        let programdata_account = Account {
            lamports:   Rent::default().minimum_balance(programdata_vec.len()),
            data:       programdata_vec,
            owner:      bpf_loader_upgradeable::ID,
            executable: false,
            rent_epoch: Epoch::default(),
        };

        // Add to both accounts to program test, now the program is deploy as upgradable
        program_test.add_account(program_key, program_account);
        program_test.add_account(programdata_key, programdata_account);


        // Start validator
        let (mut banks_client, payer, mut recent_blockhash) = program_test.start().await;

        // Transfer money to upgrade_authority so it can call the instructions
        let instruction = system_instruction::transfer(
            &payer.pubkey(),
            &upgrade_authority_keypair.pubkey(),
            1000 * LAMPORTS_PER_SOL,
        );

        let mut transaction = Transaction::new_with_payer(&[instruction], Some(&payer.pubkey()));

        let blockhash = banks_client
            .get_new_latest_blockhash(&recent_blockhash)
            .await
            .unwrap();

        recent_blockhash = blockhash;

        transaction.partial_sign(&[&payer], recent_blockhash);

        banks_client.process_transaction(transaction).await.unwrap();

        // Same as new() but here we set the payer to be upgrade_authority and program_id to be the
        // upgradable program
        return PythSimulator {
            program_id: program_key,
            banks_client,
            payer: upgrade_authority_keypair,
            last_blockhash: recent_blockhash,
            programdata_id: programdata_key,
        };
    }


    /// Process a transaction containing `instruction` signed by `signers`.
    /// The transaction is assumed to require `self.payer` to pay for and sign the transaction.
    async fn process_ix(
        &mut self,
        instruction: Instruction,
        signers: &Vec<&Keypair>,
    ) -> Result<(), BanksClientError> {
        let mut transaction =
            Transaction::new_with_payer(&[instruction], Some(&self.payer.pubkey()));

        let blockhash = self
            .banks_client
            .get_new_latest_blockhash(&self.last_blockhash)
            .await
            .unwrap();
        self.last_blockhash = blockhash;

        transaction.partial_sign(&[&self.payer], self.last_blockhash);
        transaction.partial_sign(signers, self.last_blockhash);

        self.banks_client.process_transaction(transaction).await
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

        self.process_ix(instruction, &vec![&keypair]).await.unwrap();

        keypair
    }

    /// Initialize a mapping account (using the init_mapping instruction), returning the keypair
    /// associated with the newly-created account.
    pub async fn init_mapping(&mut self) -> Result<Keypair, BanksClientError> {
        let mapping_keypair = self.create_pyth_account(size_of::<MappingAccount>()).await;

        let cmd: CommandHeader = OracleCommand::InitMapping.into();
        let instruction = Instruction::new_with_bytes(
            self.program_id,
            bytes_of(&cmd),
            vec![
                AccountMeta::new(self.payer.pubkey(), true),
                AccountMeta::new(mapping_keypair.pubkey(), true),
            ],
        );

        self.process_ix(instruction, &vec![&mapping_keypair])
            .await
            .map(|_| mapping_keypair)
    }

    /// Initialize a product account and add it to an existing mapping account (using the
    /// add_product instruction). Returns the keypair associated with the newly-created account.
    pub async fn add_product(
        &mut self,
        mapping_keypair: &Keypair,
    ) -> Result<Keypair, BanksClientError> {
        let product_keypair = self.create_pyth_account(PC_PROD_ACC_SIZE as usize).await;

        let cmd: CommandHeader = OracleCommand::AddProduct.into();
        let instruction = Instruction::new_with_bytes(
            self.program_id,
            bytes_of(&cmd),
            vec![
                AccountMeta::new(self.payer.pubkey(), true),
                AccountMeta::new(mapping_keypair.pubkey(), true),
                AccountMeta::new(product_keypair.pubkey(), true),
            ],
        );

        self.process_ix(instruction, &vec![&mapping_keypair, &product_keypair])
            .await
            .map(|_| product_keypair)
    }

    /// Delete a product account (using the del_product instruction).
    pub async fn del_product(
        &mut self,
        mapping_keypair: &Keypair,
        product_keypair: &Keypair,
    ) -> Result<(), BanksClientError> {
        let cmd: CommandHeader = OracleCommand::DelProduct.into();
        let instruction = Instruction::new_with_bytes(
            self.program_id,
            bytes_of(&cmd),
            vec![
                AccountMeta::new(self.payer.pubkey(), true),
                AccountMeta::new(mapping_keypair.pubkey(), true),
                AccountMeta::new(product_keypair.pubkey(), true),
            ],
        );

        self.process_ix(instruction, &vec![&mapping_keypair, &product_keypair])
            .await
    }

    /// Initialize a price account and add it to an existing product account (using the add_price
    /// instruction). Returns the keypair associated with the newly-created account.
    pub async fn add_price(
        &mut self,
        product_keypair: &Keypair,
        expo: i32,
    ) -> Result<Keypair, BanksClientError> {
        let price_keypair = self.create_pyth_account(size_of::<PriceAccount>()).await;

        let cmd = AddPriceArgs {
            header:     OracleCommand::AddPrice.into(),
            exponent:   expo,
            price_type: PC_PTYPE_PRICE,
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

        self.process_ix(instruction, &vec![&product_keypair, &price_keypair])
            .await
            .map(|_| price_keypair)
    }

    /// Delete a price account from an existing product account (using the del_price instruction).
    pub async fn del_price(
        &mut self,
        product_keypair: &Keypair,
        price_keypair: &Keypair,
    ) -> Result<(), BanksClientError> {
        let cmd: CommandHeader = OracleCommand::DelPrice.into();
        let instruction = Instruction::new_with_bytes(
            self.program_id,
            bytes_of(&cmd),
            vec![
                AccountMeta::new(self.payer.pubkey(), true),
                AccountMeta::new(product_keypair.pubkey(), true),
                AccountMeta::new(price_keypair.pubkey(), true),
            ],
        );

        self.process_ix(instruction, &vec![&product_keypair, &price_keypair])
            .await
    }

    /// Resize a price account (using the resize_price_account
    /// instruction).
    pub async fn resize_price_account(
        &mut self,
        price_keypair: &Keypair,
    ) -> Result<(), BanksClientError> {
        let cmd: CommandHeader = OracleCommand::ResizePriceAccount.into();
        let instruction = Instruction::new_with_bytes(
            self.program_id,
            bytes_of(&cmd),
            vec![
                AccountMeta::new(self.payer.pubkey(), true),
                AccountMeta::new(price_keypair.pubkey(), true),
                AccountMeta::new(system_program::id(), false),
            ],
        );

        self.process_ix(instruction, &vec![&price_keypair]).await
    }


    /// Update permissions (using the upd_permissions intruction) and return the pubkey of the
    /// permissions account
    pub async fn upd_permissions(
        &mut self,
        cmd_args: UpdPermissionsArgs,
    ) -> Result<Pubkey, BanksClientError> {
        let (permissions_pubkey, __bump) =
            Pubkey::find_program_address(&[PERMISSIONS_SEED.as_bytes()], &self.program_id);

        let instruction = Instruction::new_with_bytes(
            self.program_id,
            bytes_of(&cmd_args),
            vec![
                AccountMeta::new(self.payer.pubkey(), true),
                AccountMeta::new_readonly(self.program_id, false),
                AccountMeta::new_readonly(self.programdata_id, false),
                AccountMeta::new(permissions_pubkey, false),
                AccountMeta::new_readonly(system_program::id(), false),
            ],
        );

        self.process_ix(instruction, &vec![])
            .await
            .map(|_| permissions_pubkey)
    }

    /// Get the account at `key`. Returns `None` if no such account exists.
    pub async fn get_account(&mut self, key: Pubkey) -> Option<Account> {
        self.banks_client.get_account(key).await.unwrap()
    }


    /// Get the content of an account as a value of type `T`. This function returns a copy of the
    /// account data -- you cannot mutate the result to mutate the on-chain account data.
    /// Returns None if the account does not exist. Panics if the account data cannot be read as a
    /// `T`.
    pub async fn get_account_data_as<T: Pod>(&mut self, key: Pubkey) -> Option<T> {
        self.get_account(key)
            .await
            .map(|x| load::<T>(&x.data).unwrap().clone())
    }
}
