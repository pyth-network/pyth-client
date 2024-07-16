use {
    crate::{
        accounts::{
            PriceAccount,
            PriceComponent,
            PublisherCapsAccount,
            PythAccount,
            CAPS_WRITE_SEED,
        },
        c_oracle_header::PC_NUM_COMP,
        deserialize::{
            load,
            load_checked,
        },
        instruction::AddPublisherArgs,
        utils::{
            check_permissioned_funding_account,
            check_valid_funding_account,
            pyth_assert,
            send_message_to_message_buffer,
            try_convert,
            MessageBufferAccounts,
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
// account[2] caps account        [signer writable]
pub fn add_publisher(
    program_id: &Pubkey,
    accounts: &[AccountInfo],
    instruction_data: &[u8],
) -> ProgramResult {
    let cmd_args = load::<AddPublisherArgs>(instruction_data)?;

    pyth_assert(
        instruction_data.len() == size_of::<AddPublisherArgs>(),
        ProgramError::InvalidArgument,
    )?;

    let (
        funding_account,
        price_account,
        permissions_account,
        caps_account,
        maybe_accumulator_accounts,
    ) = match accounts {
        [x, y, p] => Ok((x, y, p, None, None)),
        [x, y, p, s, a, b, c, d] => Ok((
            x,
            y,
            p,
            Some(s),
            Some(MessageBufferAccounts {
                program_id:          a,
                whitelist:           b,
                oracle_auth_pda:     c,
                message_buffer_data: d,
            }),
        )),
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


    let mut price_data = load_checked::<PriceAccount>(price_account, cmd_args.header.version)?;

    // TODO: uncomment for migration
    // Use the call with the default pubkey (000..) as a trigger to update caps account
    // migration step for initializing the caps account
    // if cmd_args.publisher == Pubkey::default() {
    //     let num_comps = try_convert::<u32, usize>(price_data.num_)?;

    //     if let Some(caps_account) = caps_account {
    //         let mut caps_account =
    //             load_checked::<PublisherCapsAccount>(caps_account, cmd_args.header.version)?;
    //         caps_account.add_price(*price_account.key)?;
    //         for publisher in price_data.comp_[..num_comps].iter() {
    //             caps_account.add_publisher(publisher.pub_, *price_account.key)?;
    //         }
    //     }

    //     return Ok(());
    // }

    if price_data.num_ >= PC_NUM_COMP {
        return Err(ProgramError::InvalidArgument);
    }

    for i in 0..(try_convert::<u32, usize>(price_data.num_)?) {
        if cmd_args.publisher == price_data.comp_[i].pub_ {
            return Err(ProgramError::InvalidArgument);
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

    // Sort the publishers in the list
    #[allow(clippy::manual_swap)]
    {
        let mut comp_index = try_convert::<u32, usize>(price_data.num_ - 1)?;
        while comp_index > 0
            && price_data.comp_[comp_index - 1].pub_ > price_data.comp_[comp_index].pub_
        {
            let tmp = price_data.comp_[comp_index - 1];
            price_data.comp_[comp_index - 1] = price_data.comp_[comp_index];
            price_data.comp_[comp_index] = tmp;
            comp_index -= 1;
        }
    }

    price_data.header.size = try_convert::<_, u32>(PriceAccount::INITIAL_SIZE)?;

    if let Some(caps_account_info) = caps_account {
        let mut caps_account =
            load_checked::<PublisherCapsAccount>(caps_account_info, cmd_args.header.version)?;
        caps_account.add_publisher(cmd_args.publisher, *price_account.key)?;

        // Feature-gated accumulator-specific code, used only on pythnet/pythtest
        if let Some(accumulator_accounts) = maybe_accumulator_accounts {
            send_message_to_message_buffer(
                program_id,
                caps_account_info.key,
                CAPS_WRITE_SEED,
                accounts,
                accumulator_accounts,
                caps_account.get_caps_message(),
            )?;
        }
    }

    Ok(())
}
