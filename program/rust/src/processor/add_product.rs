use {
    crate::{
        accounts::{
            MappingAccount,
            ProductAccount,
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
            try_convert,
        },
        OracleError,
    },
    solana_program::{
        account_info::AccountInfo,
        entrypoint::ProgramResult,
        program_error::ProgramError,
        pubkey::Pubkey,
    },
    std::mem::{
        size_of,
        size_of_val,
    },
};

pub fn add_product(
    program_id: &Pubkey,
    accounts: &[AccountInfo],
    instruction_data: &[u8],
) -> ProgramResult {
    let (funding_account, tail_mapping_account, new_product_account, permissions_account_option) =
        match accounts {
            [x, y, z] => Ok((x, y, z, None)),
            [x, y, z, p] => Ok((x, y, z, Some(p))),
            _ => Err(OracleError::InvalidNumberOfAccounts),
        }?;

    let hdr = load::<CommandHeader>(instruction_data)?;

    check_valid_funding_account(funding_account)?;
    check_valid_signable_account_or_permissioned_funding_account(
        program_id,
        tail_mapping_account,
        funding_account,
        permissions_account_option,
        hdr,
    )?;
    check_valid_signable_account_or_permissioned_funding_account(
        program_id,
        new_product_account,
        funding_account,
        permissions_account_option,
        hdr,
    )?;


    let mut mapping_data = load_checked::<MappingAccount>(tail_mapping_account, hdr.version)?;
    // The mapping account must have free space to add the product account
    pyth_assert(
        mapping_data.number_of_products < PC_MAP_TABLE_SIZE,
        ProgramError::InvalidArgument,
    )?;

    ProductAccount::initialize(new_product_account, hdr.version)?;

    let current_index: usize = try_convert(mapping_data.number_of_products)?;
    mapping_data.products_list[current_index] = *new_product_account.key;
    mapping_data.number_of_products += 1;
    mapping_data.header.size = try_convert::<_, u32>(
        size_of::<MappingAccount>() - size_of_val(&mapping_data.products_list),
    )? + mapping_data.number_of_products
        * try_convert::<_, u32>(size_of::<Pubkey>())?;

    Ok(())
}
