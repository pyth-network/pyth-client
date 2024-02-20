use {
    super::test_utils::AccountSetup,
    crate::{
        accounts::{
            PriceAccount,
            PriceCumulative,
            PriceInfo,
        },
        c_oracle_header::{
            PC_MAX_SEND_LATENCY,
            PC_STATUS_TRADING,
            PC_STATUS_UNKNOWN,
        },
        deserialize::load_account_as_mut,
        error::OracleError,
    },
    quickcheck::Arbitrary,
    quickcheck_macros::quickcheck,
    solana_program::pubkey::Pubkey,
};

#[derive(Clone, Debug, Copy)]
pub struct DataEvent {
    price:       i64,
    conf:        u64,
    slot_gap:    u64,
    max_latency: u8,
}

impl Arbitrary for DataEvent {
    fn arbitrary(g: &mut quickcheck::Gen) -> Self {
        DataEvent {
            slot_gap:    u64::from(u8::arbitrary(g)) + 1, /* Slot gap is always > 1, because there
                                                           * has been a succesful aggregation */
            price:       i64::arbitrary(g),
            conf:        u64::arbitrary(g),
            max_latency: u8::arbitrary(g),
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
/// - max_latency is a random u8
#[quickcheck]
fn test_twap(input: Vec<DataEvent>) -> bool {
    let mut price_cumulative = PriceCumulative {
        price:          0,
        conf:           0,
        num_down_slots: 0,
        unused:         0,
    };

    let mut data = Vec::<DataEvent>::new();

    for data_event in input {
        price_cumulative.update(
            data_event.price,
            data_event.conf,
            data_event.slot_gap,
            data_event.max_latency,
        );
        data.push(data_event);
        price_cumulative.check_price(data.as_slice());
        price_cumulative.check_conf(data.as_slice());
        price_cumulative.check_num_down_slots(data.as_slice());
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
    pub fn check_num_down_slots(&self, data: &[DataEvent]) {
        assert_eq!(
            data.iter().fold(0, |acc, x| {
                let latency_threshold = if x.max_latency == 0 {
                    PC_MAX_SEND_LATENCY.into()
                } else {
                    x.max_latency.into()
                };
                if x.slot_gap > latency_threshold {
                    acc + (x.slot_gap - latency_threshold)
                } else {
                    acc
                }
            }),
            self.num_down_slots
        );
    }
    pub fn check_unused(&self) {
        assert_eq!(self.unused, 0);
    }
}

#[test]
fn test_twap_unit() {
    let mut price_cumulative = PriceCumulative {
        price:          1,
        conf:           2,
        num_down_slots: 3,
        unused:         0,
    };

    let data = vec![
        DataEvent {
            price:       1,
            conf:        2,
            slot_gap:    4,
            max_latency: 0,
        },
        DataEvent {
            price:       i64::MAX,
            conf:        u64::MAX,
            slot_gap:    1,
            max_latency: 0,
        },
        DataEvent {
            price:       -10,
            conf:        4,
            slot_gap:    30,
            max_latency: 0,
        },
        DataEvent {
            price:       1,
            conf:        2,
            slot_gap:    4,
            max_latency: 5,
        },
        DataEvent {
            price:       6,
            conf:        7,
            slot_gap:    8,
            max_latency: 5,
        },
    ];

    price_cumulative.update(
        data[0].price,
        data[0].conf,
        data[0].slot_gap,
        data[0].max_latency,
    );
    assert_eq!(price_cumulative.price, 5);
    assert_eq!(price_cumulative.conf, 10);
    assert_eq!(price_cumulative.num_down_slots, 3);
    assert_eq!(price_cumulative.unused, 0);

    price_cumulative.update(
        data[1].price,
        data[1].conf,
        data[1].slot_gap,
        data[1].max_latency,
    );
    assert_eq!(price_cumulative.price, 9_223_372_036_854_775_812i128);
    assert_eq!(price_cumulative.conf, 18_446_744_073_709_551_625u128);
    assert_eq!(price_cumulative.num_down_slots, 3);
    assert_eq!(price_cumulative.unused, 0);

    price_cumulative.update(
        data[2].price,
        data[2].conf,
        data[2].slot_gap,
        data[2].max_latency,
    );
    assert_eq!(price_cumulative.price, 9_223_372_036_854_775_512i128);
    assert_eq!(price_cumulative.conf, 18_446_744_073_709_551_745u128);
    assert_eq!(price_cumulative.num_down_slots, 8);
    assert_eq!(price_cumulative.unused, 0);

    let mut price_cumulative_overflow = PriceCumulative {
        price:          0,
        conf:           0,
        num_down_slots: 0,
        unused:         0,
    };
    price_cumulative_overflow.update(i64::MIN, u64::MAX, u64::MAX, u8::MAX);
    assert_eq!(
        price_cumulative_overflow.price,
        i128::MIN - i128::from(i64::MIN)
    );
    assert_eq!(
        price_cumulative_overflow.conf,
        u128::MAX - 2 * u128::from(u64::MAX)
    );
    assert_eq!(
        price_cumulative_overflow.num_down_slots,
        u64::MAX - u64::from(u8::MAX)
    );
    assert_eq!(price_cumulative_overflow.unused, 0);

    let mut price_cumulative_nonzero_max_latency = PriceCumulative {
        price:          1,
        conf:           2,
        num_down_slots: 3,
        unused:         0,
    };

    price_cumulative_nonzero_max_latency.update(
        data[3].price,
        data[3].conf,
        data[3].slot_gap,
        data[3].max_latency,
    );
    assert_eq!(price_cumulative_nonzero_max_latency.price, 5);
    assert_eq!(price_cumulative_nonzero_max_latency.conf, 10);
    assert_eq!(price_cumulative_nonzero_max_latency.num_down_slots, 3);
    assert_eq!(price_cumulative_nonzero_max_latency.unused, 0);

    price_cumulative_nonzero_max_latency.update(
        data[4].price,
        data[4].conf,
        data[4].slot_gap,
        data[4].max_latency,
    );
    assert_eq!(price_cumulative_nonzero_max_latency.price, 53);
    assert_eq!(price_cumulative_nonzero_max_latency.conf, 66);
    assert_eq!(price_cumulative_nonzero_max_latency.num_down_slots, 6);
    assert_eq!(price_cumulative_nonzero_max_latency.unused, 0);
}

#[test]
fn test_twap_with_price_account() {
    let program_id = Pubkey::new_unique();
    let mut price_setup: AccountSetup = AccountSetup::new::<PriceAccount>(&program_id);
    let price_account = price_setup.as_account_info();

    // Normal behavior
    let mut price_data = load_account_as_mut::<PriceAccount>(&price_account).unwrap();
    price_data.agg_ = PriceInfo {
        price_:           -10,
        conf_:            5,
        status_:          PC_STATUS_TRADING,
        corp_act_status_: 0,
        pub_slot_:        5,
    };
    price_data.price_cumulative = PriceCumulative {
        price:          1,
        conf:           2,
        num_down_slots: 3,
        unused:         0,
    };
    price_data.prev_slot_ = 3;
    price_data.update_price_cumulative().unwrap();

    assert_eq!(price_data.price_cumulative.price, 1 - 2 * 10);
    assert_eq!(price_data.price_cumulative.conf, 2 + 2 * 5);
    assert_eq!(price_data.price_cumulative.num_down_slots, 3);

    // Slot decreases
    price_data.agg_ = PriceInfo {
        price_:           300,
        conf_:            6,
        status_:          PC_STATUS_TRADING,
        corp_act_status_: 0,
        pub_slot_:        1,
    };
    price_data.prev_slot_ = 5;

    assert_eq!(price_data.price_cumulative.price, 1 - 2 * 10);
    assert_eq!(price_data.price_cumulative.conf, 2 + 2 * 5);
    assert_eq!(price_data.price_cumulative.num_down_slots, 3);

    // Status is not trading
    price_data.agg_ = PriceInfo {
        price_:           1,
        conf_:            2,
        status_:          PC_STATUS_UNKNOWN,
        corp_act_status_: 0,
        pub_slot_:        6,
    };
    price_data.prev_slot_ = 5;
    assert_eq!(
        price_data.update_price_cumulative(),
        Err(OracleError::NeedsSuccesfulAggregation)
    );

    assert_eq!(price_data.price_cumulative.price, 1 - 2 * 10);
    assert_eq!(price_data.price_cumulative.conf, 2 + 2 * 5);
    assert_eq!(price_data.price_cumulative.num_down_slots, 3);

    // Back to normal behavior
    price_data.agg_.status_ = PC_STATUS_TRADING;
    price_data.update_price_cumulative().unwrap();

    assert_eq!(price_data.price_cumulative.price, 1 - 2 * 10 + 1);
    assert_eq!(price_data.price_cumulative.conf, 2 + 2 * 5 + 2);
    assert_eq!(price_data.price_cumulative.num_down_slots, 3);
}
