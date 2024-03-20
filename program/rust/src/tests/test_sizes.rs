use {
    crate::{
        accounts::{
            AccountHeader,
            MappingAccount,
            PermissionAccount,
            PriceAccount,
            PriceComponent,
            PriceEma,
            PriceInfo,
            ProductAccount,
            PythAccount,
        },
        c_oracle_header::{
            PC_MAP_TABLE_SIZE,
            PC_NUM_COMP,
            PC_VERSION,
            ZSTD_UPPER_BOUND,
        },
        deserialize::{
            load,
            load_checked,
        },
        instruction::{
            AddPriceArgs,
            AddPublisherArgs,
            CommandHeader,
            DelPublisherArgs,
            InitPriceArgs,
            SetMaxLatencyArgs,
            SetMinPubArgs,
            UpdPriceArgs,
        },
        tests::test_utils::AccountSetup,
        utils::try_convert,
    },
    solana_program::pubkey::Pubkey,
    std::mem::{
        size_of,
        size_of_val,
    },
};
#[test]
fn test_sizes() {
    assert_eq!(size_of::<Pubkey>(), 32);
    assert_eq!(
        size_of::<MappingAccount>(),
        24 + (PC_MAP_TABLE_SIZE as usize + 1) * size_of::<Pubkey>()
    );
    assert_eq!(size_of::<PriceInfo>(), 32);
    assert_eq!(
        size_of::<PriceComponent>(),
        size_of::<Pubkey>() + 2 * size_of::<PriceInfo>()
    );

    #[cfg(feature = "pythnet")]
    {
        use crate::{
            accounts::PriceCumulative,
            c_oracle_header::PC_NUM_COMP_PYTHNET,
        };

        // Sanity-check the Pythnet PC_NUM_COMP
        assert_eq!(PC_NUM_COMP, 64);

        assert_eq!(
            size_of::<PriceAccount>(),
            48 + u64::BITS as usize
                + 3 * size_of::<Pubkey>()
                + size_of::<PriceInfo>()
                + (PC_NUM_COMP_PYTHNET as usize) * size_of::<PriceComponent>()
                + 4096                             // Size of unused_
                + 1024                             // Size of unused2_
                + 512                              // Size of unused3_
                + 256                              // Size of unused4_
                + 128                              // Size of unused5_
                + 32                               // Size of unused6_
                + size_of::<PriceEma>()
                + size_of::<PriceEma>()
                + size_of::<PriceCumulative>()
                + size_of::<PriceCumulative>()
        );
        assert_eq!(size_of::<PriceAccount>(), 12576);
        assert!(size_of::<PriceAccount>() == try_convert::<_, usize>(ZSTD_UPPER_BOUND).unwrap());

        assert_eq!(size_of::<PriceCumulative>(), 48);
    }

    #[cfg(not(feature = "pythnet"))]
    {
        use crate::c_oracle_header::PC_NUM_COMP_SOLANA;

        // Sanity-check the Solana PC_NUM_COMP
        assert_eq!(PC_NUM_COMP, PC_NUM_COMP_SOLANA);

        assert_eq!(
            size_of::<PriceAccount>(),
            48 + u64::BITS as usize
                + 3 * size_of::<Pubkey>()
                + size_of::<PriceInfo>()
                + (PC_NUM_COMP_SOLANA as usize) * size_of::<PriceComponent>()
        );
        assert_eq!(size_of::<PriceAccount>(), 3312);
        assert!(size_of::<PriceAccount>() <= try_convert::<_, usize>(ZSTD_UPPER_BOUND).unwrap());
    }

    assert_eq!(size_of::<CommandHeader>(), 8);
    assert_eq!(size_of::<AddPriceArgs>(), 16);
    assert_eq!(size_of::<InitPriceArgs>(), 16);
    assert_eq!(size_of::<SetMinPubArgs>(), 12);
    assert_eq!(size_of::<SetMaxLatencyArgs>(), 12);
    assert_eq!(size_of::<AddPublisherArgs>(), 40);
    assert_eq!(size_of::<DelPublisherArgs>(), 40);
    assert_eq!(size_of::<UpdPriceArgs>(), 40);
    assert_eq!(size_of::<Pubkey>(), 32);
    assert_eq!(size_of::<AccountHeader>(), 16);
    assert_eq!(size_of::<MappingAccount>(), 20536);
    assert_eq!(size_of::<ProductAccount>(), 48);
    assert_eq!(size_of::<PriceComponent>(), 96);
    assert_eq!(size_of::<PriceEma>(), 24);
    assert_eq!(size_of::<PermissionAccount>(), 112);
}

#[test]
fn test_offsets() {
    let program_id = Pubkey::new_unique();

    let mut mapping_setup = AccountSetup::new::<MappingAccount>(&program_id);
    let mapping_account = mapping_setup.as_account_info();

    MappingAccount::initialize(&mapping_account, PC_VERSION).unwrap();
    let mapping_data = load_checked::<MappingAccount>(&mapping_account, PC_VERSION).unwrap();

    assert_eq!(
        size_of::<MappingAccount>() - size_of_val(&mapping_data.products_list),
        try_convert::<_, usize>(MappingAccount::INITIAL_SIZE).unwrap()
    );
}

#[test]
fn test_pubkey() {
    let default_pubkey = Pubkey::default();
    let zero_pubkey = Pubkey::new(&[0u8; 32]);
    let unique_pubkey = Pubkey::new_unique();

    assert_eq!(default_pubkey, zero_pubkey);
    assert!(unique_pubkey != default_pubkey);

    // Check that current onchain bytes are interpretable as a Solana Pubkey. I manually grabbed the
    // bytes for BTC/USD price account's product field with `solana account` and check that they
    // get properly interpreted as the actual base58 product account
    let onchain_bytes =
        hex::decode("3515b3861e8fe93e5f540ba4077c216404782b86d5e78077b3cbfd27313ab3bc").unwrap();
    let onchain_bytes_as_solana_pubkey = load::<Pubkey>(onchain_bytes.as_slice()).unwrap();
    assert_eq!(
        onchain_bytes_as_solana_pubkey.to_string(),
        "4aDoSXJ5o3AuvL7QFeR6h44jALQfTmUUCTVGDD6aoJTM"
    );
}
