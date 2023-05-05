use {
    crate::accounts::{
        PriceFeedMessage,
        TWAPMessage,
    },
    byteorder::{
        BigEndian,
        ReadBytesExt,
    },
    quickcheck::Arbitrary,
    quickcheck_macros::quickcheck,
    std::io::{
        Cursor,
        Read,
    },
};

// TODO: test serialization and deserialization of PriceFeedMessage

impl Arbitrary for PriceFeedMessage {
    fn arbitrary(g: &mut quickcheck::Gen) -> Self {
        let mut id = [0u8; 32];
        for item in &mut id {
            *item = u8::arbitrary(g);
        }

        let publish_time = i64::arbitrary(g);

        PriceFeedMessage {
            id,
            price: i64::arbitrary(g),
            conf: u64::arbitrary(g),
            exponent: i32::arbitrary(g),
            publish_time,
            prev_publish_time: publish_time.saturating_sub(i64::arbitrary(g)),
            ema_price: i64::arbitrary(g),
            ema_conf: u64::arbitrary(g),
        }
    }
}

impl Arbitrary for TWAPMessage {
    fn arbitrary(g: &mut quickcheck::Gen) -> Self {
        let mut id = [0u8; 32];
        for item in &mut id {
            *item = u8::arbitrary(g);
        }

        let publish_time = i64::arbitrary(g);

        TWAPMessage {
            id,
            cumulative_price: i128::arbitrary(g),
            cumulative_conf: u128::arbitrary(g),
            num_gaps: u64::arbitrary(g),
            exponent: i32::arbitrary(g),
            publish_time,
            prev_publish_time: publish_time.saturating_sub(i64::arbitrary(g)),
            publish_slot: u64::arbitrary(g),
        }
    }
}

#[quickcheck]
fn test_price_feed_message_roundtrip(input: PriceFeedMessage) -> bool {
    let reconstructed = PriceFeedMessage::from_bytes(&input.as_bytes());

    println!("Failed test case:");
    println!("{:?}", input);
    println!("{:?}", reconstructed);

    reconstructed.is_some() && reconstructed.unwrap() == input
}

#[quickcheck]
fn test_twap_message_roundtrip(input: TWAPMessage) -> bool {
    let reconstructed = TWAPMessage::from_bytes(&input.as_bytes());

    println!("Failed test case:");
    println!("{:?}", input);
    println!("{:?}", reconstructed);

    reconstructed.is_some() && reconstructed.unwrap() == input
}


impl TWAPMessage {
    fn from_bytes(bytes: &[u8]) -> Option<Self> {
        let mut cursor = Cursor::new(bytes);

        let discriminator = cursor.read_u8().ok()?;
        if discriminator != 1 {
            return None;
        }

        let mut id = [0u8; 32];
        cursor.read_exact(&mut id).ok()?;

        let cumulative_price = cursor.read_i128::<BigEndian>().ok()?;
        let cumulative_conf = cursor.read_u128::<BigEndian>().ok()?;
        let num_gaps = cursor.read_u64::<BigEndian>().ok()?;
        let exponent = cursor.read_i32::<BigEndian>().ok()?;
        let publish_time = cursor.read_i64::<BigEndian>().ok()?;
        let prev_publish_time = cursor.read_i64::<BigEndian>().ok()?;
        let publish_slot = cursor.read_u64::<BigEndian>().ok()?;

        if cursor.position() as usize != bytes.len() {
            // The message bytes are longer than expected
            None
        } else {
            Some(Self {
                id,
                cumulative_price,
                cumulative_conf,
                num_gaps,
                exponent,
                publish_time,
                prev_publish_time,
                publish_slot,
            })
        }
    }
}

/// TODO: move this parsing implementation to a separate data format library.
impl PriceFeedMessage {
    fn from_bytes(bytes: &[u8]) -> Option<Self> {
        let mut cursor = Cursor::new(bytes);

        let discriminator = cursor.read_u8().ok()?;
        if discriminator != 0 {
            return None;
        }

        let mut id = [0u8; 32];
        cursor.read_exact(&mut id).ok()?;

        let price = cursor.read_i64::<BigEndian>().ok()?;
        let conf = cursor.read_u64::<BigEndian>().ok()?;
        let exponent = cursor.read_i32::<BigEndian>().ok()?;
        let publish_time = cursor.read_i64::<BigEndian>().ok()?;
        let prev_publish_time = cursor.read_i64::<BigEndian>().ok()?;
        let ema_price = cursor.read_i64::<BigEndian>().ok()?;
        let ema_conf = cursor.read_u64::<BigEndian>().ok()?;


        if cursor.position() as usize != bytes.len() {
            // The message bytes are longer than expected
            None
        } else {
            Some(Self {
                id,
                price,
                conf,
                exponent,
                publish_time,
                prev_publish_time,
                ema_price,
                ema_conf,
            })
        }
    }
}
