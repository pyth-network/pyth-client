use {
    crate::{
        accounts::{
            PriceAccount,
            PriceEma,
            PriceInfo,
        },
        deserialize::{
            load,
            load_checked,
        },
        instruction::InitPriceArgs,
        utils::{
            check_exponent_range,
            check_valid_funding_account,
            check_valid_signable_account_or_permissioned_funding_account,
            pyth_assert,
        },
        OracleError,
    },
    bytemuck::bytes_of_mut,
    solana_program::{
        account_info::AccountInfo,
        entrypoint::ProgramResult,
        program_error::ProgramError,
        program_memory::sol_memset,
        pubkey::Pubkey,
    },
    std::mem::size_of,
};

/// (Re)initialize price account
// account[0] funding account       [signer writable]
// account[1] new price account     [signer writable]
pub fn init_price(
    program_id: &Pubkey,
    accounts: &[AccountInfo],
    instruction_data: &[u8],
) -> ProgramResult {
    let cmd_args = load::<InitPriceArgs>(instruction_data)?;

    check_exponent_range(cmd_args.exponent)?;

    let (funding_account, price_account, permissions_account_option) = match accounts {
        [x, y] => Ok((x, y, None)),
        [x, y, p] => Ok((x, y, Some(p))),
        _ => Err(OracleError::InvalidNumberOfAccounts),
    }?;

    check_valid_funding_account(funding_account)?;
    check_valid_signable_account_or_permissioned_funding_account(
        program_id,
        price_account,
        funding_account,
        permissions_account_option,
        &cmd_args.header,
    )?;


    let mut price_data = load_checked::<PriceAccount>(price_account, cmd_args.header.version)?;
    pyth_assert(
        price_data.price_type == cmd_args.price_type,
        ProgramError::InvalidArgument,
    )?;

    price_data.exponent = cmd_args.exponent;

    price_data.last_slot_ = 0;
    price_data.valid_slot_ = 0;
    price_data.agg_.pub_slot_ = 0;
    price_data.prev_slot_ = 0;
    price_data.prev_price_ = 0;
    price_data.prev_conf_ = 0;
    price_data.prev_timestamp_ = 0;
    sol_memset(
        bytes_of_mut(&mut price_data.twap_),
        0,
        size_of::<PriceEma>(),
    );
    sol_memset(
        bytes_of_mut(&mut price_data.twac_),
        0,
        size_of::<PriceEma>(),
    );
    sol_memset(
        bytes_of_mut(&mut price_data.agg_),
        0,
        size_of::<PriceInfo>(),
    );
    for i in 0..price_data.comp_.len() {
        sol_memset(
            bytes_of_mut(&mut price_data.comp_[i].agg_),
            0,
            size_of::<PriceInfo>(),
        );
        sol_memset(
            bytes_of_mut(&mut price_data.comp_[i].latest_),
            0,
            size_of::<PriceInfo>(),
        );
    }

    Ok(())
}
