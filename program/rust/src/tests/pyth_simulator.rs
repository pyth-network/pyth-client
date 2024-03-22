use {
    crate::{
        accounts::{
            MappingAccount,
            PERMISSIONS_SEED,
        },
        c_oracle_header::{
            PC_PROD_ACC_SIZE,
            PC_PTYPE_PRICE,
        },
        deserialize::load,
        instruction::{
            AddPriceArgs,
            AddPublisherArgs,
            CommandHeader,
            OracleCommand,
            UpdPermissionsArgs,
            UpdPriceArgs,
        },
    },
    bytemuck::{
        bytes_of,
        Pod,
    },
    serde::{
        Deserialize,
        Serialize,
    },
    solana_program::{
        bpf_loader_upgradeable::{
            self,
            UpgradeableLoaderState,
        },
        clock::Clock,
        hash::Hash,
        instruction::{
            AccountMeta,
            Instruction,
        },
        native_token::LAMPORTS_PER_SOL,
        pubkey::Pubkey,
        rent::Rent,
        stake_history::Epoch,
        system_instruction,
        system_program,
        sysvar::SysvarId,
    },
    solana_program_test::{
        read_file,
        BanksClientError,
        ProgramTest,
        ProgramTestBanksClientExt,
        ProgramTestContext,
        ProgramTestError,
    },
    solana_sdk::{
        account::Account,
        signature::{
            Keypair,
            Signer,
        },
        transaction::Transaction,
    },
    std::{
        collections::HashMap,
        fs::File,
        iter::once,
        mem::size_of,
        path::PathBuf,
    },
};

lazy_static::lazy_static! {
    // Build the oracle binary and make it available to the
    // simulator. lazy_static makes this happen only once per test
    // run.
    static ref ORACLE_PROGRAM_BINARY_PATH: PathBuf = {

    // Detect features and pass them onto cargo-build-bpf.
    // IMPORTANT: All features of this crate must have gates added to this vector.
    let features: Vec<&str> = vec!["pythnet"];

    let mut cmd = std::process::Command::new("cargo");
    cmd.arg("build-bpf");

    if !features.is_empty() {
        cmd.arg("--features");
        cmd.args(features);
    }

    let status = cmd.status().unwrap();

    if !status.success() {
        panic!(
        "cargo-build-bpf did not exit with 0 (code {:?})",
        status.code()
        );
    }

    let target_dir = concat!(env!("CARGO_MANIFEST_DIR"), "/../../target");

    PathBuf::from(target_dir).join("deploy/pyth_oracle.so")
    };
}

/// Simulator for the state of the pyth program on Solana. You can run solana transactions against
/// this struct to test how pyth instructions execute in the Solana runtime.
pub struct PythSimulator {
    program_id:            Pubkey,
    context:               ProgramTestContext,
    /// Hash used to submit the last transaction. The hash must be advanced for each new
    /// transaction; otherwise, replayed transactions in different states can return stale
    /// results.
    last_blockhash:        Hash,
    programdata_id:        Pubkey,
    pub upgrade_authority: Keypair,
    pub genesis_keypair:   Keypair,
}

pub struct Quote {
    pub price:      i64,
    pub confidence: u64,
    pub status:     u32,
}

#[derive(Debug, Deserialize, Serialize)]
struct ProductMetadata {
    symbol:         String,
    asset_type:     String,
    country:        String,
    quote_currency: String,
    tenor:          String,
}

impl PythSimulator {
    /// Deploys the oracle program as upgradable
    pub async fn new() -> PythSimulator {
        let mut bpf_data = read_file(&*ORACLE_PROGRAM_BINARY_PATH);

        let mut program_test = ProgramTest::default();
        let program_key = Pubkey::new_unique();
        // This PDA is the actual address in the real world
        // https://docs.rs/solana-program/1.6.4/solana_program/bpf_loader_upgradeable/index.html
        let (programdata_key, _) =
            Pubkey::find_program_address(&[&program_key.to_bytes()], &bpf_loader_upgradeable::id());

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

        // Add accounts to program test, now the program is deploy as upgradable
        program_test.add_account(program_key, program_account);
        program_test.add_account(programdata_key, programdata_account);

        // Start validator
        let context = program_test.start_with_context().await;
        let genesis_keypair = copy_keypair(&context.payer);
        let last_blockhash = context.last_blockhash;
        let mut result = PythSimulator {
            program_id: program_key,
            context,
            last_blockhash,
            programdata_id: programdata_key,
            upgrade_authority: upgrade_authority_keypair,
            genesis_keypair: copy_keypair(&genesis_keypair),
        };

        // Transfer money to upgrade_authority so it can call the instructions
        result
            .airdrop(&result.upgrade_authority.pubkey(), 1000 * LAMPORTS_PER_SOL)
            .await
            .unwrap();

        result
            .upd_permissions(
                UpdPermissionsArgs {
                    header:                  OracleCommand::UpdPermissions.into(),
                    master_authority:        genesis_keypair.pubkey(),
                    data_curation_authority: genesis_keypair.pubkey(),
                    security_authority:      genesis_keypair.pubkey(),
                },
                &copy_keypair(&result.upgrade_authority),
            )
            .await
            .unwrap();

        result
    }

    /// Process a transaction containing `instructions` signed by `signers`.
    /// `payer` is used to pay for and sign the transaction.
    async fn process_ixs(
        &mut self,
        instructions: &[Instruction],
        signers: &Vec<&Keypair>,
        payer: &Keypair,
    ) -> Result<(), BanksClientError> {
        let mut transaction = Transaction::new_with_payer(instructions, Some(&payer.pubkey()));

        let blockhash = self
            .context
            .banks_client
            .get_new_latest_blockhash(&self.last_blockhash)
            .await
            .unwrap();
        self.last_blockhash = blockhash;

        transaction.partial_sign(&[payer], self.last_blockhash);
        transaction.partial_sign(signers, self.last_blockhash);

        self.context
            .banks_client
            .process_transaction(transaction)
            .await
    }

    /// Create an account owned by the pyth program containing `size` bytes.
    /// The account will be created with enough lamports to be rent-exempt.
    pub async fn create_pyth_account(&mut self, size: usize) -> Keypair {
        let keypair = Keypair::new();
        let rent = Rent::minimum_balance(&Rent::default(), size);
        let instruction = system_instruction::create_account(
            &self.genesis_keypair.pubkey(),
            &keypair.pubkey(),
            rent,
            size as u64,
            &self.program_id,
        );

        self.process_ixs(
            &[instruction],
            &vec![&keypair],
            &copy_keypair(&self.genesis_keypair),
        )
        .await
        .unwrap();

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
                AccountMeta::new(self.genesis_keypair.pubkey(), true),
                AccountMeta::new(mapping_keypair.pubkey(), true),
                AccountMeta::new(self.get_permissions_pubkey(), false),
            ],
        );

        self.process_ixs(
            &[instruction],
            &vec![&mapping_keypair],
            &copy_keypair(&self.genesis_keypair),
        )
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
                AccountMeta::new(self.genesis_keypair.pubkey(), true),
                AccountMeta::new(mapping_keypair.pubkey(), true),
                AccountMeta::new(product_keypair.pubkey(), true),
                AccountMeta::new(self.get_permissions_pubkey(), false),
            ],
        );

        self.process_ixs(
            &[instruction],
            &vec![mapping_keypair, &product_keypair],
            &copy_keypair(&self.genesis_keypair),
        )
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
                AccountMeta::new(self.genesis_keypair.pubkey(), true),
                AccountMeta::new(mapping_keypair.pubkey(), true),
                AccountMeta::new(product_keypair.pubkey(), true),
                AccountMeta::new(self.get_permissions_pubkey(), false),
            ],
        );

        self.process_ixs(
            &[instruction],
            &vec![mapping_keypair, product_keypair],
            &copy_keypair(&self.genesis_keypair),
        )
        .await
    }

    /// Initialize a price account and add it to an existing product account (using the add_price
    /// instruction). Returns the keypair associated with the newly-created account.
    pub async fn add_price(
        &mut self,
        product_keypair: &Keypair,
        expo: i32,
    ) -> Result<Keypair, BanksClientError> {
        let price_keypair = self
            .create_pyth_account(size_of::<crate::accounts::PriceAccount>())
            .await;

        let cmd = AddPriceArgs {
            header:     OracleCommand::AddPrice.into(),
            exponent:   expo,
            price_type: PC_PTYPE_PRICE,
        };
        let instruction = Instruction::new_with_bytes(
            self.program_id,
            bytes_of(&cmd),
            vec![
                AccountMeta::new(self.genesis_keypair.pubkey(), true),
                AccountMeta::new(product_keypair.pubkey(), true),
                AccountMeta::new(price_keypair.pubkey(), true),
                AccountMeta::new(self.get_permissions_pubkey(), false),
            ],
        );

        self.process_ixs(
            &[instruction],
            &vec![product_keypair, &price_keypair],
            &copy_keypair(&self.genesis_keypair),
        )
        .await
        .map(|_| price_keypair)
    }

    /// Add a publisher to a price account (using the add_publisher instruction).
    pub async fn add_publisher(
        &mut self,
        price_keypair: &Keypair,
        publisher: Pubkey,
    ) -> Result<(), BanksClientError> {
        let cmd = AddPublisherArgs {
            header: OracleCommand::AddPublisher.into(),
            publisher,
        };
        let instruction = Instruction::new_with_bytes(
            self.program_id,
            bytes_of(&cmd),
            vec![
                AccountMeta::new(self.genesis_keypair.pubkey(), true),
                AccountMeta::new(price_keypair.pubkey(), true),
                AccountMeta::new(self.get_permissions_pubkey(), false),
            ],
        );

        self.process_ixs(
            &[instruction],
            &vec![price_keypair],
            &copy_keypair(&self.genesis_keypair),
        )
        .await
    }

    /// Update price of a component price account (using the upd_price instruction).
    pub async fn upd_price(
        &mut self,
        publisher: &Keypair,
        price_account: Pubkey,
        quote: Quote,
    ) -> Result<(), BanksClientError> {
        self.upd_price_batch(
            publisher,
            &HashMap::from_iter(once(("".to_string(), price_account))),
            &HashMap::from_iter(once(("".to_string(), quote))),
        )
        .await
    }

    /// Update price in multiple price account atomically (using the upd_price instruction)
    pub async fn upd_price_batch(
        &mut self,
        publisher: &Keypair,
        price_accounts: &HashMap<String, Pubkey>,
        quotes: &HashMap<String, Quote>,
    ) -> Result<(), BanksClientError> {
        let slot = self.context.banks_client.get_sysvar::<Clock>().await?.slot;
        let mut instructions: Vec<Instruction> = vec![];

        for (key, price_account) in price_accounts {
            let cmd = UpdPriceArgs {
                header:          OracleCommand::UpdPrice.into(),
                status:          quotes[key].status,
                unused_:         0,
                price:           quotes[key].price,
                confidence:      quotes[key].confidence,
                publishing_slot: slot,
            };
            instructions.push(Instruction::new_with_bytes(
                self.program_id,
                bytes_of(&cmd),
                vec![
                    AccountMeta::new(publisher.pubkey(), true),
                    AccountMeta::new(*price_account, false),
                    AccountMeta::new(Clock::id(), false),
                ],
            ));
        }

        self.process_ixs(&instructions, &vec![publisher], publisher)
            .await
    }

    // /// Delete a price account from an existing product account (using the del_price instruction).
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
                AccountMeta::new(self.genesis_keypair.pubkey(), true),
                AccountMeta::new(product_keypair.pubkey(), true),
                AccountMeta::new(price_keypair.pubkey(), true),
                AccountMeta::new(self.get_permissions_pubkey(), false),
            ],
        );

        self.process_ixs(
            &[instruction],
            &vec![product_keypair, price_keypair],
            &copy_keypair(&self.genesis_keypair),
        )
        .await
    }

    /// Update permissions (using the upd_permissions intruction) and return the pubkey of the
    /// permissions account
    pub async fn upd_permissions(
        &mut self,
        cmd_args: UpdPermissionsArgs,
        payer: &Keypair,
    ) -> Result<Pubkey, BanksClientError> {
        let permissions_pubkey = self.get_permissions_pubkey();

        let instruction = Instruction::new_with_bytes(
            self.program_id,
            bytes_of(&cmd_args),
            vec![
                AccountMeta::new(payer.pubkey(), true),
                AccountMeta::new_readonly(self.programdata_id, false),
                AccountMeta::new(permissions_pubkey, false),
                AccountMeta::new_readonly(system_program::id(), false),
            ],
        );

        self.process_ixs(&[instruction], &vec![], payer)
            .await
            .map(|_| permissions_pubkey)
    }

    /// Get the account at `key`. Returns `None` if no such account exists.
    pub async fn get_account(&mut self, key: Pubkey) -> Option<Account> {
        self.context.banks_client.get_account(key).await.unwrap()
    }

    /// Get the content of an account as a value of type `T`. This function returns a copy of the
    /// account data -- you cannot mutate the result to mutate the on-chain account data.
    /// Returns None if the account does not exist. Panics if the account data cannot be read as a
    /// `T`.
    pub async fn get_account_data_as<T: Pod>(&mut self, key: Pubkey) -> Option<T> {
        self.get_account(key)
            .await
            .map(|x| *load::<T>(&x.data).unwrap())
    }

    pub fn is_owned_by_oracle(&self, account: &Account) -> bool {
        account.owner == self.program_id
    }

    pub async fn airdrop(&mut self, to: &Pubkey, lamports: u64) -> Result<(), BanksClientError> {
        let instruction =
            system_instruction::transfer(&self.genesis_keypair.pubkey(), to, lamports);

        self.process_ixs(
            &[instruction],
            &vec![],
            &copy_keypair(&self.genesis_keypair),
        )
        .await
    }

    pub fn get_permissions_pubkey(&self) -> Pubkey {
        let (permissions_pubkey, __bump) =
            Pubkey::find_program_address(&[PERMISSIONS_SEED.as_bytes()], &self.program_id);
        permissions_pubkey
    }

    /// Setup 3 product accounts with 1 price account each and add a publisher to all of them.
    /// Returns the mapping of product symbol to price account pubkey.
    /// TODO : this fixture doesn't set the product metadata
    pub async fn setup_product_fixture(
        &mut self,
        publishers: &[Pubkey],
        security_authority: Pubkey,
    ) -> HashMap<String, Pubkey> {
        let result_file =
            File::open("./test_data/publish/products.json").expect("Test file not found");

        for publisher in publishers {
            self.airdrop(publisher, 100 * LAMPORTS_PER_SOL)
                .await
                .unwrap();
        }

        self.airdrop(&security_authority, 100 * LAMPORTS_PER_SOL)
            .await
            .unwrap();

        let product_metadatas: HashMap<String, ProductMetadata> =
            serde_json::from_reader(&result_file).unwrap();

        let mut price_accounts: HashMap<String, Pubkey> = HashMap::new();
        let mapping_keypair: Keypair = self.init_mapping().await.unwrap();
        for symbol in product_metadatas.keys() {
            let product_keypair = self.add_product(&mapping_keypair).await.unwrap();
            let price_keypair = self.add_price(&product_keypair, -5).await.unwrap();
            for publisher in publishers.iter() {
                self.add_publisher(&price_keypair, *publisher)
                    .await
                    .unwrap();
            }
            price_accounts.insert(symbol.to_string(), price_keypair.pubkey());
        }
        price_accounts
    }

    /// Advance clock to slot `slot`.
    pub async fn warp_to_slot(&mut self, slot: u64) -> Result<(), ProgramTestError> {
        self.context.warp_to_slot(slot)
    }
}

pub fn copy_keypair(keypair: &Keypair) -> Keypair {
    Keypair::from_bytes(&keypair.to_bytes()).unwrap()
}
