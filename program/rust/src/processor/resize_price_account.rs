use {
    crate::{
        accounts::{
            PriceAccount,
            PythAccount,
        },
        c_oracle_header::PC_VERSION,
        deserialize::{
            load,
            load_checked,
        },
        instruction::CommandHeader,
        time_machine_types::PriceAccountWrapper,
        utils::{
            check_valid_funding_account,
            check_valid_signable_account_or_permissioned_funding_account,
            get_rent,
            pyth_assert,
            send_lamports,
        },
        OracleError,
    },
    solana_program::{
        account_info::AccountInfo,
        entrypoint::ProgramResult,
        program_error::ProgramError,
        pubkey::Pubkey,
        rent::Rent,
        system_program::check_id,
    },
    std::mem::size_of,
};

/// Resizes a price account so that it fits the Time Machine
// account[0] funding account       [signer writable]
// account[1] price account         [signer writable]
// account[2] system program        []
pub fn resize_price_account(
    program_id: &Pubkey,
    accounts: &[AccountInfo],
    instruction_data: &[u8],
) -> ProgramResult {
    let (funding_account, price_account, system_program, permissions_account_option) =
        match accounts {
            [x, y, z] => Ok((x, y, z, None)),
            [x, y, z, p] => Ok((x, y, z, Some(p))),
            _ => Err(OracleError::InvalidNumberOfAccounts),
        }?;

    let hdr = load::<CommandHeader>(instruction_data)?;

    check_valid_funding_account(funding_account)?;
    check_valid_signable_account_or_permissioned_funding_account(
        program_id,
        price_account,
        funding_account,
        permissions_account_option,
        hdr,
    )?;
    pyth_assert(
        check_id(system_program.key),
        OracleError::InvalidSystemAccount.into(),
    )?;
    // Check that it is a valid initialized price account
    {
        load_checked::<PriceAccount>(price_account, PC_VERSION)?;
    }
    let account_len = price_account.try_data_len()?;
    match account_len {
        PriceAccount::MINIMUM_SIZE => {
            // Ensure account is still rent exempt after resizing
            let rent: Rent = get_rent()?;
            let lamports_needed: u64 = rent
                .minimum_balance(size_of::<PriceAccountWrapper>())
                .saturating_sub(price_account.lamports());
            if lamports_needed > 0 {
                send_lamports(
                    funding_account,
                    price_account,
                    system_program,
                    lamports_needed,
                )?;
            }
            // We do not need to zero allocate because we won't access the data in the same
            // instruction
            price_account.realloc(size_of::<PriceAccountWrapper>(), false)?;

            // Check that everything is ok
            check_valid_signable_account_or_permissioned_funding_account(
                program_id,
                price_account,
                funding_account,
                permissions_account_option,
                hdr,
            )?;

            #[cfg(test)]
            // Sma feature disabled except in tests
            {
                let mut price_account =
                    load_checked::<PriceAccountWrapper>(price_account, PC_VERSION)?;
                // Initialize Time Machine
                price_account.initialize_time_machine()?;
            }

            Ok(())
        }
        PriceAccountWrapper::MINIMUM_SIZE => Ok(()),
        _ => Err(ProgramError::InvalidArgument),
    }
}