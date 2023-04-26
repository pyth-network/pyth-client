use {
    crate::{
        accounts::{
            PriceAccount,
            PriceFeedPayload,
            PythAccount,
        },
        c_oracle_header::{
            PC_STATUS_IGNORED,
            PC_STATUS_TRADING,
            PC_STATUS_UNKNOWN,
            PC_VERSION,
        },
        deserialize::{
            load_checked,
            load_mut,
        },
        instruction::{
            OracleCommand,
            UpdPriceArgs,
        },
        processor::process_instruction,
        tests::test_utils::{
            update_clock_slot,
            AccountSetup,
        },
    },
    itertools::Itertools,
    quickcheck::Arbitrary,
    quickcheck_macros::quickcheck,
    solana_program::{
        program_error::ProgramError,
        pubkey::Pubkey,
    },
    std::mem::size_of,
    borsh::ser::BorshSerialize,
};


#[quickcheck]
fn test_upd_price_cpi(discriminator: u64, messages: Vec<Vec<u8>>) {
    /*
    let account_key = Pubkey::new_unique();

    let message = PriceFeedPayload {
        id:           account_key.to_bytes(),
        price:        -1234,
        conf:         45,
        exponent:     -8,
        publish_time: 787574,
        ema_price:    -1354,
        ema_conf:     2409,
    }
    .as_bytes()
    .to_vec();

    // anchor discriminator for "global:put_all"
    let discriminator: [u8; 8] = [212, 225, 193, 91, 151, 238, 20, 93];
*/

    let price_account_key = Pubkey::new_unique().to_bytes();

    let borsh_bytes = (discriminator.to_be_bytes(), price_account_key, &messages)
        .try_to_vec()
        .unwrap();
    let my_serialization = my_borsh_serialize(&discriminator.to_be_bytes(), &price_account_key, &messages.iter().map(|v| v.as_slice()).collect_vec());

    println!("{:?}", borsh_bytes);
    println!("{:?}", my_serialization);

    assert_eq!(my_serialization, borsh_bytes);
}


fn my_borsh_serialize(
    discriminator: &[u8; 8],
    account_key: &[u8; 32],
    messages: &[&[u8]]
) -> Vec<u8> {
    let mut my_serialization: Vec<u8> = vec![];

    my_serialization.extend_from_slice(discriminator);
    my_serialization.extend_from_slice(account_key);
    my_serialization.extend_from_slice(&(messages.len() as u32).to_le_bytes());

    for message in messages {
        my_serialization.extend_from_slice(&(message.len() as u32).to_le_bytes());
        my_serialization.extend_from_slice(&message);
    }

    my_serialization
}