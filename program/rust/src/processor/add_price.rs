use {
    crate::{
        accounts::{
            PriceAccount,
            ProductAccount,
            PythAccount,
        },
        c_oracle_header::PC_PTYPE_UNKNOWN,
        deserialize::{
            load,
            load_checked,
        },
        instruction::AddPriceArgs,
        utils::{
            check_exponent_range,
            check_valid_funding_account,
            check_valid_signable_account_or_permissioned_funding_account,
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


    let (funding_account, product_account, price_account, permissions_account_option) =
        match accounts {
            [x, y, z] => Ok((x, y, z, None)),
            [x, y, z, p] => Ok((x, y, z, Some(p))),
            _ => Err(OracleError::InvalidNumberOfAccounts),
        }?;

    check_valid_funding_account(funding_account)?;
    check_valid_signable_account_or_permissioned_funding_account(
        program_id,
        product_account,
        funding_account,
        permissions_account_option,
        &cmd_args.header,
    )?;
    check_valid_signable_account_or_permissioned_funding_account(
        program_id,
        price_account,
        funding_account,
        permissions_account_option,
        &cmd_args.header,
    )?;

    let mut product_data =
        load_checked::<ProductAccount>(product_account, cmd_args.header.version)?;

    let mut price_data = PriceAccount::initialize(price_account, cmd_args.header.version)?;
    price_data.exponent = cmd_args.exponent;
    price_data.price_type = cmd_args.price_type;
    price_data.product_account = *product_account.key;
    price_data.next_price_account = product_data.first_price_account;
    product_data.first_price_account = *price_account.key;

    Ok(())
}
