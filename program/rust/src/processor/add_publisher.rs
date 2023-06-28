use {
    crate::{
        accounts::{
            PriceAccount,
            PriceComponent,
            PythAccount,
        },
        c_oracle_header::PC_COMP_SIZE,
        deserialize::{
            load,
            load_checked,
        },
        instruction::AddPublisherArgs,
        utils::{
            check_valid_funding_account,
            check_valid_signable_account_or_permissioned_funding_account,
            pyth_assert,
            try_convert,
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

/// Add publisher to symbol account
// account[0] funding account       [signer writable]
// account[1] price account         [signer writable]
pub fn add_publisher(
    program_id: &Pubkey,
    accounts: &[AccountInfo],
    instruction_data: &[u8],
) -> ProgramResult {
    let cmd_args = load::<AddPublisherArgs>(instruction_data)?;

    pyth_assert(
        instruction_data.len() == size_of::<AddPublisherArgs>()
            && cmd_args.publisher != Pubkey::default(),
        ProgramError::InvalidArgument,
    )?;

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

    if price_data.num_ >= PC_COMP_SIZE {
        return Ok(());
    }

    for i in 0..(try_convert::<u32, usize>(price_data.num_)?) {
        if cmd_args.publisher == price_data.comp_[i].pub_ {
            return Ok(());
        }
    }

    let current_index: usize = try_convert(price_data.num_)?;
    sol_memset(
        bytes_of_mut(&mut price_data.comp_[current_index]),
        0,
        size_of::<PriceComponent>(),
    );
    price_data.comp_[current_index].pub_ = cmd_args.publisher;
    price_data.num_ += 1;
    price_data.header.size = try_convert::<_, u32>(PriceAccount::INITIAL_SIZE)?;
    Ok(())
}
