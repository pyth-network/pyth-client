use {
    crate::{
        accounts::{
            PermissionAccount,
            PriceAccount,
            PriceAccountFlags,
            PythAccount,
        },
        c_oracle_header::{
            PC_STATUS_TRADING,
            PC_STATUS_UNKNOWN,
            PC_VERSION,
        },
        deserialize::{
            load_checked,
            load_mut,
        },
        instruction::{
            AddPublisherArgs,
            OracleCommand,
            UpdPriceArgs,
        },
        processor::{
            process_instruction,
            ALLOW_ZERO_CI,
            FORBID_ZERO_CI,
        },
        tests::test_utils::{
            update_clock_slot,
            AccountSetup,
        },
    },
    bytemuck::bytes_of,
    solana_program::pubkey::Pubkey,
    std::mem::size_of,
};

struct Accounts {
    program_id:          Pubkey,
    publisher_account:   AccountSetup,
    funding_account:     AccountSetup,
    price_account:       AccountSetup,
    permissions_account: AccountSetup,
    clock_account:       AccountSetup,
}

impl Accounts {
    fn new() -> Self {
        let program_id = Pubkey::new_unique();
        let publisher_account = AccountSetup::new_funding();
        let clock_account = AccountSetup::new_clock();
        let mut funding_account = AccountSetup::new_funding();
        let mut permissions_account = AccountSetup::new_permission(&program_id);
        let mut price_account = AccountSetup::new::<PriceAccount>(&program_id);

        PriceAccount::initialize(&price_account.as_account_info(), PC_VERSION).unwrap();

        {
            let permissions_account_info = permissions_account.as_account_info();
            let mut permissions_account_data =
                PermissionAccount::initialize(&permissions_account_info, PC_VERSION).unwrap();
            permissions_account_data.master_authority = *funding_account.as_account_info().key;
            permissions_account_data.data_curation_authority =
                *funding_account.as_account_info().key;
            permissions_account_data.security_authority = *funding_account.as_account_info().key;
        }

        Self {
            program_id,
            publisher_account,
            funding_account,
            price_account,
            permissions_account,
            clock_account,
        }
    }
}

fn add_publisher(accounts: &mut Accounts, publisher: Option<Pubkey>) {
    let args = AddPublisherArgs {
        header:    OracleCommand::AddPublisher.into(),
        publisher: publisher.unwrap_or(*accounts.publisher_account.as_account_info().key),
    };

    assert!(process_instruction(
        &accounts.program_id,
        &[
            accounts.funding_account.as_account_info(),
            accounts.price_account.as_account_info(),
            accounts.permissions_account.as_account_info(),
        ],
        bytes_of::<AddPublisherArgs>(&args)
    )
    .is_ok());
}

fn update_price(accounts: &mut Accounts, price: i64, conf: u64, slot: u64) {
    let instruction_data = &mut [0u8; size_of::<UpdPriceArgs>()];
    let mut cmd = load_mut::<UpdPriceArgs>(instruction_data).unwrap();
    cmd.header = OracleCommand::UpdPrice.into();
    cmd.status = PC_STATUS_TRADING;
    cmd.price = price;
    cmd.confidence = conf;
    cmd.publishing_slot = slot;
    cmd.unused_ = 0;

    let mut clock = accounts.clock_account.as_account_info();
    clock.is_signer = false;
    clock.is_writable = false;

    process_instruction(
        &accounts.program_id,
        &[
            accounts.publisher_account.as_account_info(),
            accounts.price_account.as_account_info(),
            clock,
        ],
        instruction_data,
    )
    .unwrap();
}

#[test]
fn test_aggregate_v2_toggle() {
    let accounts = &mut Accounts::new();

    // Add an initial Publisher to test with.
    add_publisher(accounts, None);

    // Update the price, no aggregation will happen on the first slot.
    {
        update_clock_slot(&mut accounts.clock_account.as_account_info(), 1);
        update_price(accounts, 42, 2, 1);
        let info = accounts.price_account.as_account_info();
        let price_data = load_checked::<PriceAccount>(&info, PC_VERSION).unwrap();
        assert_eq!(price_data.last_slot_, 0);
        assert!(!price_data.flags.contains(PriceAccountFlags::ACCUMULATOR_V2));
    }

    // Update again, component is now TRADING so aggregation should trigger.
    {
        update_clock_slot(&mut accounts.clock_account.as_account_info(), 2);
        update_price(accounts, 43, 3, 2);
        let info = accounts.price_account.as_account_info();
        let price_data = load_checked::<PriceAccount>(&info, PC_VERSION).unwrap();
        assert_eq!(price_data.agg_.status_, PC_STATUS_TRADING);
        assert_eq!(price_data.last_slot_, 2);
        assert_eq!(price_data.agg_.price_, 42);
        assert_eq!(price_data.agg_.conf_, 2);
    }

    // Update again, but with confidence set to 0, it should not aggregate *in the next slot*.
    {
        update_clock_slot(&mut accounts.clock_account.as_account_info(), 3);
        update_price(accounts, 44, 0, 3);
        let info = accounts.price_account.as_account_info();
        let price_data = load_checked::<PriceAccount>(&info, PC_VERSION).unwrap();
        assert_eq!(price_data.agg_.status_, PC_STATUS_TRADING);
        assert_eq!(price_data.last_slot_, 3);
        assert_eq!(price_data.agg_.price_, 43);
        assert_eq!(price_data.agg_.conf_, 3);
    }

    // Update again, to trigger aggregation. We should see status go to unknown
    {
        update_clock_slot(&mut accounts.clock_account.as_account_info(), 4);
        update_price(accounts, 45, 0, 4);
        let info = accounts.price_account.as_account_info();
        let price_data = load_checked::<PriceAccount>(&info, PC_VERSION).unwrap();
        println!("Price Data: {:?}", price_data.agg_);
        assert_eq!(price_data.agg_.status_, PC_STATUS_UNKNOWN);
        assert_eq!(price_data.last_slot_, 3);
    }

    // Enable allow zero confidence bit
    add_publisher(accounts, Some(ALLOW_ZERO_CI.into()));

    // Update again, with allow zero confidence bit set, aggregation should support
    // zero confidence values. Note that we don't need to do this twice, because the
    // price with ci zero was already stored in the previous slot.
    {
        update_clock_slot(&mut accounts.clock_account.as_account_info(), 5);
        update_price(accounts, 46, 0, 5);
        let info = accounts.price_account.as_account_info();
        let price_data = load_checked::<PriceAccount>(&info, PC_VERSION).unwrap();
        assert!(price_data.flags.contains(PriceAccountFlags::ALLOW_ZERO_CI));
        assert_eq!(price_data.agg_.status_, PC_STATUS_TRADING);
        assert_eq!(price_data.last_slot_, 5);
        assert_eq!(price_data.agg_.price_, 45);
        assert_eq!(price_data.agg_.conf_, 0);
    }

    // Disable allow zero confidence bit
    add_publisher(accounts, Some(FORBID_ZERO_CI.into()));

    // Update again, with forbid zero confidence bit set, aggregation should have status
    // of unknown
    {
        update_clock_slot(&mut accounts.clock_account.as_account_info(), 6);
        update_price(accounts, 47, 0, 6);
        let info = accounts.price_account.as_account_info();
        let price_data = load_checked::<PriceAccount>(&info, PC_VERSION).unwrap();
        assert!(!price_data.flags.contains(PriceAccountFlags::ALLOW_ZERO_CI));
        assert_eq!(price_data.agg_.status_, PC_STATUS_UNKNOWN);
    }
}
