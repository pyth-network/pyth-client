use {
    crate::accounts::PythOracleSerialize,
    byteorder::BigEndian,
    pythnet_sdk::{
        messages::{
            Message,
            PriceFeedMessage,
            TwapMessage,
        },
        wire::from_slice,
    },
    quickcheck_macros::quickcheck,
};

#[quickcheck]
fn test_price_feed_message_roundtrip(input: PriceFeedMessage) -> bool {
    let reconstructed = from_slice::<BigEndian, Message>(&input.to_bytes()).unwrap();

    println!("Failed test case:");
    println!("{:?}", input);
    println!("{:?}", reconstructed);
    match reconstructed {
        Message::PriceFeedMessage(reconstructed) => reconstructed == input,
        _ => false,
    }
}

#[quickcheck]
fn test_twap_message_roundtrip(input: TwapMessage) -> bool {
    let reconstructed = from_slice::<BigEndian, TwapMessage>(&input.to_bytes()).unwrap();

    println!("Failed test case:");
    println!("{:?}", input);
    println!("{:?}", reconstructed);

    reconstructed == input
}
