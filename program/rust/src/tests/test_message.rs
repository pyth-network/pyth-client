use {
    crate::accounts::PythOracleSerialize,
    byteorder::BigEndian,
    pythnet_sdk::{
        messages::{
            Message,
            PriceFeedMessage,
            PublisherCapsMessage,
            TwapMessage,
        },
        wire::{
            from_slice,
            Serializer,
        },
    },
    quickcheck::{
        Gen,
        QuickCheck,
    },
    quickcheck_macros::quickcheck,
    serde::Serialize,
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
    let reconstructed = from_slice::<BigEndian, Message>(&input.to_bytes()).unwrap();

    println!("Failed test case:");
    println!("{:?}", input);
    println!("{:?}", reconstructed);

    match reconstructed {
        Message::TwapMessage(reconstructed) => reconstructed == input,
        _ => false,
    }
}


fn prop_publisher_caps_message_roundtrip(input: PublisherCapsMessage) -> bool {
    let reconstructed = from_slice::<BigEndian, Message>(&input.clone().to_bytes()).unwrap();

    println!("Failed test case:");
    println!("{:?}", input);
    println!("{:?}", reconstructed);

    match reconstructed {
        Message::PublisherCapsMessage(reconstructed) => reconstructed == input,
        _ => false,
    }
}

#[test]
fn test_publisher_caps_message_roundtrip() {
    // Configure the size parameter for the generator
    QuickCheck::new()
        .gen(Gen::new(1024))
        .quickcheck(prop_publisher_caps_message_roundtrip as fn(PublisherCapsMessage) -> bool);
}
