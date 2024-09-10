use {
    crate::{
        accounts::{
            AccountHeader,
            MappingAccount,
            PythAccount,
        },
        c_oracle_header::PC_MAGIC,
        deserialize::{
            load,
            load_account_as,
        },
        instruction::CommandHeader,
        utils::{
            check_valid_writable_account,
            pyth_assert,
        },
        OracleError,
    },
    solana_program::{
        account_info::AccountInfo,
        entrypoint::{
            ProgramResult,
            MAX_PERMITTED_DATA_INCREASE,
        },
        pubkey::Pubkey,
    },
    std::{
        cmp::min,
        mem::size_of,
    },
};

/// Resize mapping account.
// account[1] mapping account [writable]
pub fn resize_mapping(
    program_id: &Pubkey,
    accounts: &[AccountInfo],
    instruction_data: &[u8],
) -> ProgramResult {
    let mapping_account = match accounts {
        [x] => Ok(x),
        _ => Err(OracleError::InvalidNumberOfAccounts),
    }?;

    let hdr = load::<CommandHeader>(instruction_data)?;

    check_valid_writable_account(program_id, mapping_account)?;

    {
        let account_header = load_account_as::<AccountHeader>(mapping_account)?;
        pyth_assert(
            account_header.magic_number == PC_MAGIC
                && account_header.version == hdr.version
                && account_header.account_type == MappingAccount::ACCOUNT_TYPE,
            OracleError::InvalidAccountHeader.into(),
        )?;
    }

    pyth_assert(
        mapping_account.data_len() < size_of::<MappingAccount>(),
        OracleError::NoNeedToResize.into(),
    )?;
    let new_size = min(
        size_of::<MappingAccount>(),
        mapping_account.data_len() + MAX_PERMITTED_DATA_INCREASE,
    );
    mapping_account.realloc(new_size, true)?;

    Ok(())
}
