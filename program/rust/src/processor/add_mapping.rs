use {
    crate::{
        accounts::{
            MappingAccount,
            PythAccount,
        },
        c_oracle_header::PC_MAP_TABLE_SIZE,
        deserialize::{
            load,
            load_checked,
        },
        instruction::CommandHeader,
        utils::{
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

pub fn add_mapping(
    program_id: &Pubkey,
    accounts: &[AccountInfo],
    instruction_data: &[u8],
) -> ProgramResult {
    let (funding_account, cur_mapping, next_mapping, permissions_account_option) = match accounts {
        [x, y, z] => Ok((x, y, z, None)),
        [x, y, z, p] => Ok((x, y, z, Some(p))),
        _ => Err(OracleError::InvalidNumberOfAccounts),
    }?;

    let hdr = load::<CommandHeader>(instruction_data)?;

    check_valid_funding_account(funding_account)?;
    check_valid_signable_account_or_permissioned_funding_account(
        program_id,
        cur_mapping,
        funding_account,
        permissions_account_option,
        hdr,
    )?;
    check_valid_signable_account_or_permissioned_funding_account(
        program_id,
        next_mapping,
        funding_account,
        permissions_account_option,
        hdr,
    )?;

    let mut cur_mapping = load_checked::<MappingAccount>(cur_mapping, hdr.version)?;
    pyth_assert(
        cur_mapping.number_of_products == PC_MAP_TABLE_SIZE
            && cur_mapping.next_mapping_account == Pubkey::default(),
        ProgramError::InvalidArgument,
    )?;

    MappingAccount::initialize(next_mapping, hdr.version)?;
    cur_mapping.next_mapping_account = *next_mapping.key;

    Ok(())
}
