use {
    crate::{
        accounts::{
            PermissionAccount,
            PriceAccount,
            PythAccount,
        },
        c_oracle_header::PC_VERSION,
        deserialize::{
            load_checked,
            load_mut,
        },
        instruction::{
            OracleCommand,
            SetMaxLatencyArgs,
        },
        processor::set_max_latency,
        tests::test_utils::AccountSetup,
    },
    solana_program::{
        account_info::AccountInfo,
        program_error::ProgramError,
        pubkey::Pubkey,
    },
    std::mem::size_of,
};

#[test]
fn test_set_max_latency() {
    let mut instruction_data = [0u8; size_of::<SetMaxLatencyArgs>()];

    let program_id = Pubkey::new_unique();

    let mut funding_setup = AccountSetup::new_funding();
    let funding_account = funding_setup.as_account_info();

    let mut price_setup = AccountSetup::new::<PriceAccount>(&program_id);
    let price_account = price_setup.as_account_info();
    PriceAccount::initialize(&price_account, PC_VERSION).unwrap();

    let mut permissions_setup = AccountSetup::new_permission(&program_id);
    let permissions_account = permissions_setup.as_account_info();

    {
        let mut permissions_account_data =
            PermissionAccount::initialize(&permissions_account, PC_VERSION).unwrap();
        permissions_account_data.master_authority = *funding_account.key;
        permissions_account_data.data_curation_authority = *funding_account.key;
        permissions_account_data.security_authority = *funding_account.key;
    }

    assert_eq!(get_max_latency(&price_account), Ok(0));

    populate_instruction(&mut instruction_data, 10);
    assert!(set_max_latency(
        &program_id,
        &[
            funding_account.clone(),
            price_account.clone(),
            permissions_account.clone()
        ],
        &instruction_data
    )
    .is_ok());
    assert_eq!(get_max_latency(&price_account), Ok(10));

    populate_instruction(&mut instruction_data, 5);
    assert!(set_max_latency(
        &program_id,
        &[
            funding_account.clone(),
            price_account.clone(),
            permissions_account.clone()
        ],
        &instruction_data
    )
    .is_ok());
    assert_eq!(get_max_latency(&price_account), Ok(5));
}

// Populate the instruction data with SetMaxLatencyArgs
fn populate_instruction(instruction_data: &mut [u8], max_latency: u8) {
    let mut hdr = load_mut::<SetMaxLatencyArgs>(instruction_data).unwrap();
    hdr.header = OracleCommand::SetMaxLatency.into();
    hdr.max_latency = max_latency;
}

// Helper function to get the max latency from a PriceAccount
fn get_max_latency(account: &AccountInfo) -> Result<u8, ProgramError> {
    Ok(load_checked::<PriceAccount>(account, PC_VERSION)?.max_latency_)
}
