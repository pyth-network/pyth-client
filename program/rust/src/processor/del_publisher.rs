use {
    crate::{
        accounts::{
            PriceAccount,
            PriceComponent,
            PublisherCapsAccount,
            PythAccount,
        },
        deserialize::{
            load,
            load_checked,
        },
        instruction::DelPublisherArgs,
        utils::{
            check_permissioned_funding_account,
            check_valid_funding_account,
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

/// Delete publisher from symbol account
// account[0] funding account       [signer writable]
// account[1] price account         [signer writable]
// account[2] scores account        [signer writable]
pub fn del_publisher(
    program_id: &Pubkey,
    accounts: &[AccountInfo],
    instruction_data: &[u8],
) -> ProgramResult {
    let cmd_args = load::<DelPublisherArgs>(instruction_data)?;

    pyth_assert(
        instruction_data.len() == size_of::<DelPublisherArgs>()
            && cmd_args.publisher != Pubkey::default(),
        ProgramError::InvalidArgument,
    )?;

    let (funding_account, price_account, permissions_account, scores_account) = match accounts {
        [x, y, p] => Ok((x, y, p, None)),
        [x, y, p, s] => Ok((x, y, p, Some(s))),
        _ => Err(OracleError::InvalidNumberOfAccounts),
    }?;

    check_valid_funding_account(funding_account)?;
    check_permissioned_funding_account(
        program_id,
        price_account,
        funding_account,
        permissions_account,
        &cmd_args.header,
    )?;

    if let Some(scores_account) = scores_account {
        let mut scores_account =
            load_checked::<PublisherCapsAccount>(scores_account, cmd_args.header.version)?;
        scores_account.del_publisher(cmd_args.publisher, *price_account.key)?;
    }

    let mut price_data = load_checked::<PriceAccount>(price_account, cmd_args.header.version)?;

    for i in 0..(try_convert::<u32, usize>(price_data.num_)?) {
        if cmd_args.publisher == price_data.comp_[i].pub_ {
            for j in i + 1..(try_convert::<u32, usize>(price_data.num_)?) {
                price_data.comp_[j - 1] = price_data.comp_[j];
            }
            price_data.num_ -= 1;
            let current_index: usize = try_convert(price_data.num_)?;
            sol_memset(
                bytes_of_mut(&mut price_data.comp_[current_index]),
                0,
                size_of::<PriceComponent>(),
            );
            price_data.header.size = try_convert::<_, u32>(PriceAccount::INITIAL_SIZE)?;
            return Ok(());
        }
    }
    Err(ProgramError::InvalidArgument)
}
