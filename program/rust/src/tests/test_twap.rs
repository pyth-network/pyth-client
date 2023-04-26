use {
    crate::{
        accounts::PriceCumulative,
        c_oracle_header::PC_MAX_SEND_LATENCY,
    },
    quickcheck::Arbitrary,
    quickcheck_macros::quickcheck,
};

#[derive(Clone, Debug, Copy)]
pub struct DataEvent {
    price:    i64,
    conf:     u64,
    slot_gap: u64,
}

impl Arbitrary for DataEvent {
    fn arbitrary(g: &mut quickcheck::Gen) -> Self {
        DataEvent {
            slot_gap: u64::from(u8::arbitrary(g)) + 1, /* Slot gap is always > 1, because there
                                                        * has been a succesful aggregation */
            price:    i64::arbitrary(g),
            conf:     u64::arbitrary(g),
        }
    }
}

/// This is a generative test for the PriceCumulative struct. quickcheck will generate a series of
/// vectors of DataEvents of different length. The generation is based on the arbitrary trait
/// above.
/// For each DataEvent :
/// - slot_gap is a random number between 1 and u8::MAX + 1 (256)
/// - price is a random i64
/// - conf is a random u64
#[quickcheck]
fn test_twap(input: Vec<DataEvent>) -> bool {
    let mut price_cumulative = PriceCumulative {
        price:    0,
        conf:     0,
        num_gaps: 0,
        unused:   0,
    };

    let mut data = Vec::<DataEvent>::new();

    for data_event in input {
        price_cumulative.update(data_event.price, data_event.conf, data_event.slot_gap);
        data.push(data_event);
        price_cumulative.check_price(data.as_slice());
        price_cumulative.check_conf(data.as_slice());
        price_cumulative.check_num_gaps(data.as_slice());
        price_cumulative.check_unused();
    }

    true
}


impl PriceCumulative {
    pub fn check_price(&self, data: &[DataEvent]) {
        assert_eq!(
            data.iter()
                .map(|x| i128::from(x.price) * i128::from(x.slot_gap))
                .sum::<i128>(),
            self.price
        );
    }
    pub fn check_conf(&self, data: &[DataEvent]) {
        assert_eq!(
            data.iter()
                .map(|x| u128::from(x.conf) * u128::from(x.slot_gap))
                .sum::<u128>(),
            self.conf
        );
    }
    pub fn check_num_gaps(&self, data: &[DataEvent]) {
        assert_eq!(
            data.iter()
                .fold(0, |acc, x| if x.slot_gap > PC_MAX_SEND_LATENCY.into() {
                    acc + 1
                } else {
                    acc
                }),
            self.num_gaps
        );
    }
    pub fn check_unused(&self) {
        assert_eq!(self.unused, 0);
    }
}

#[test]
fn test_twap_unit() {
    let mut price_cumulative = PriceCumulative {
        price:    1,
        conf:     2,
        num_gaps: 3,
        unused:   0,
    };

    let data = vec![
        DataEvent {
            price:    1,
            conf:     2,
            slot_gap: 4,
        },
        DataEvent {
            price:    i64::MAX,
            conf:     u64::MAX,
            slot_gap: 1,
        },
    ];

    price_cumulative.update(data[0].price, data[0].conf, data[0].slot_gap);
    assert_eq!(price_cumulative.price, 5);
    assert_eq!(price_cumulative.conf, 10);
    assert_eq!(price_cumulative.num_gaps, 3);
    assert_eq!(price_cumulative.unused, 0);

    price_cumulative.update(data[1].price, data[1].conf, data[1].slot_gap);
    assert_eq!(price_cumulative.price, 9_223_372_036_854_775_812i128);
    assert_eq!(price_cumulative.conf, 18_446_744_073_709_551_625u128);
    assert_eq!(price_cumulative.num_gaps, 3);
    assert_eq!(price_cumulative.unused, 0);

    let mut price_cumulative_overflow = PriceCumulative {
        price:    0,
        conf:     0,
        num_gaps: 0,
        unused:   0,
    };
    price_cumulative_overflow.update(i64::MIN, u64::MAX, u64::MAX);
    assert_eq!(
        price_cumulative_overflow.price,
        i128::MIN - i128::from(i64::MIN)
    );
    assert_eq!(
        price_cumulative_overflow.conf,
        u128::MAX - 2 * u128::from(u64::MAX)
    );
    assert_eq!(price_cumulative_overflow.num_gaps, 1);
    assert_eq!(price_cumulative_overflow.unused, 0);
}
