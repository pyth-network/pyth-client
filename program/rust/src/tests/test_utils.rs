use solana_program::account_info::AccountInfo;
use solana_program::system_program;
use solana_program::native_token::LAMPORTS_PER_SOL;
use solana_program::pubkey::Pubkey;
use solana_program::clock::Epoch;

fn setup_funding_account<'a>(& mut funding_balance : u64) -> AccountInfo<'a> {
    let mut funding_balance = LAMPORTS_PER_SOL.clone();
    static system_program : Pubkey = system_program::ID;
    static funding_key : Pubkey = Pubkey::new_unique();

    let funding_account = AccountInfo::new(
        &funding_key,
        true,
        true,
        funding_balance,
        &mut [],
        &system_program,
        false,
        Epoch::default(),
    );

    return funding_account
}
