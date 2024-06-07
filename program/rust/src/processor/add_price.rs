use {
    crate::{
        accounts::{
            PriceAccount,
            ProductAccount,
            PublisherScoresAccount,
            PythAccount,
        },
        c_oracle_header::{
            PC_PTYPE_UNKNOWN,
            PRICE_ACCOUNT_DEFAULT_MIN_PUB,
        },
        deserialize::{
            load,
            load_checked,
        },
        instruction::AddPriceArgs,
        utils::{
            check_exponent_range,
            check_permissioned_funding_account,
            check_valid_funding_account,
            pyth_assert,
        },
        OracleError,
    },
    solana_program::{
        account_info::AccountInfo,
        entrypoint::ProgramResult,
        program_error::ProgramError,
        pubkey::Pubkey,
    },
};

/// Add new price account to a product account
// account[0] funding account       [signer writable]
// account[1] product account       [signer writable]
// account[2] new price account     [signer writable]
// account[3] scores account        [signer writable]
pub fn add_price(
    program_id: &Pubkey,
    accounts: &[AccountInfo],
    instruction_data: &[u8],
) -> ProgramResult {
    let cmd_args = load::<AddPriceArgs>(instruction_data)?;

    check_exponent_range(cmd_args.exponent)?;
    pyth_assert(
        cmd_args.price_type != PC_PTYPE_UNKNOWN,
        ProgramError::InvalidArgument,
    )?;


    let (funding_account, product_account, price_account, permissions_account, scores_account) =
        match accounts {
            [x, y, z, p] => Ok((x, y, z, p, None)),
            [x, y, z, p, s] => Ok((x, y, z, p, Some(s))),
            _ => Err(OracleError::InvalidNumberOfAccounts),
        }?;

    check_valid_funding_account(funding_account)?;
    check_permissioned_funding_account(
        program_id,
        product_account,
        funding_account,
        permissions_account,
        &cmd_args.header,
    )?;
    check_permissioned_funding_account(
        program_id,
        price_account,
        funding_account,
        permissions_account,
        &cmd_args.header,
    )?;

    let mut product_data =
        load_checked::<ProductAccount>(product_account, cmd_args.header.version)?;

    let mut price_data = PriceAccount::initialize(price_account, cmd_args.header.version)?;
    price_data.exponent = cmd_args.exponent;
    price_data.price_type = cmd_args.price_type;
    price_data.product_account = *product_account.key;
    price_data.next_price_account = product_data.first_price_account;
    price_data.min_pub_ = PRICE_ACCOUNT_DEFAULT_MIN_PUB;
    product_data.first_price_account = *price_account.key;

    if let Some(scores_account) = scores_account {
        let mut scores_account =
            load_checked::<PublisherScoresAccount>(scores_account, cmd_args.header.version)?;
        scores_account.add_price(*price_account.key)?;
    }

    Ok(())
}
