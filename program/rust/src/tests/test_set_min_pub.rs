use {
    crate::{
        accounts::{
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
            SetMinPubArgs,
        },
        processor::process_instruction,
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
fn test_set_min_pub() {
    let mut instruction_data = [0u8; size_of::<SetMinPubArgs>()];

    let program_id = Pubkey::new_unique();

    let mut funding_setup = AccountSetup::new_funding();
    let funding_account = funding_setup.as_account_info();

    let mut price_setup = AccountSetup::new::<PriceAccount>(&program_id);
    let price_account = price_setup.as_account_info();
    PriceAccount::initialize(&price_account, PC_VERSION).unwrap();

    assert_eq!(get_min_pub(&price_account), Ok(0));

    populate_instruction(&mut instruction_data, 10);
    assert!(process_instruction(
        &program_id,
        &[funding_account.clone(), price_account.clone()],
        &instruction_data
    )
    .is_ok());
    assert_eq!(get_min_pub(&price_account), Ok(10));

    populate_instruction(&mut instruction_data, 2);
    assert!(process_instruction(
        &program_id,
        &[funding_account.clone(), price_account.clone()],
        &instruction_data
    )
    .is_ok());
    assert_eq!(get_min_pub(&price_account), Ok(2));
}

// Create an upd_product instruction that sets the product metadata to strings
fn populate_instruction(instruction_data: &mut [u8], min_pub: u8) {
    let mut hdr = load_mut::<SetMinPubArgs>(instruction_data).unwrap();
    hdr.header = OracleCommand::SetMinPub.into();
    hdr.minimum_publishers = min_pub;
}

fn get_min_pub(account: &AccountInfo) -> Result<u8, ProgramError> {
    Ok(load_checked::<PriceAccount>(account, PC_VERSION)?.min_pub_)
}
