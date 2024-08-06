use {
    super::reserve_new_price_feed_index,
    crate::{
        accounts::{
            PriceAccount,
            ProductAccount,
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
            check_valid_writable_account,
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
// account[0] funding account        [signer writable]
// account[1] product account        [writable]
// account[2] new price account      [writable]
// account[3] permissions account    [writable]
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

    let (funding_account, product_account, price_account, permissions_account) = match accounts {
        [x, y, z, p] => Ok((x, y, z, p)),
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
    check_valid_writable_account(program_id, permissions_account)?;

    let mut product_data =
        load_checked::<ProductAccount>(product_account, cmd_args.header.version)?;

    // TODO: where is it created???
    let mut price_data = PriceAccount::initialize(price_account, cmd_args.header.version)?;
    price_data.exponent = cmd_args.exponent;
    price_data.price_type = cmd_args.price_type;
    price_data.product_account = *product_account.key;
    price_data.next_price_account = product_data.first_price_account;
    price_data.min_pub_ = PRICE_ACCOUNT_DEFAULT_MIN_PUB;
    price_data.feed_index = reserve_new_price_feed_index(permissions_account)?;
    product_data.first_price_account = *price_account.key;

    Ok(())
}
