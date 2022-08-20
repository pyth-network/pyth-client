use solana_program_test::BanksClient;
use solana_sdk::signature::Keypair;

struct PythSimulator {
  mut banks_client: BanksClient,
  payer: Keypair,

}