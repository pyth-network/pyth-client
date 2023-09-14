#![cfg(feature = "price_v2_resize")]

use {
    crate::{
        accounts::{
            PriceAccount,
            PriceAccountV2,
        },
        c_oracle_header::PC_VERSION,
        deserialize::{
            load,
            load_checked,
        },
        instruction::CommandHeader,
        utils::{
            check_valid_funding_account,
            check_valid_signable_account_or_permissioned_funding_account,
            get_rent,
            pyth_assert,
            send_lamports,
            try_convert,
        },
        OracleError,
    },
    solana_program::{
        account_info::AccountInfo,
        entrypoint::ProgramResult,
        pubkey::Pubkey,
        rent::Rent,
        system_program::check_id,
    },
    std::mem::size_of,
};

// Increase the size of an account allocated for a PriceAccount such that it can hold a PriceAccountV2
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
    if account_len < size_of::<PriceAccountV2>() {
        // Ensure account is still rent exempt after resizing
        let rent: Rent = get_rent()?;
        let lamports_needed: u64 = rent
            .minimum_balance(size_of::<PriceAccountV2>())
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
        price_account.realloc(size_of::<PriceAccountV2>(), false)?;

        // Check that we can still load the account, update header size for homogeneity
        {
            let mut price_data = load_checked::<PriceAccount>(price_account, PC_VERSION)?;
            price_data.header.size = try_convert(size_of::<PriceAccountV2>())?;
        }
    }

    Ok(())
}
