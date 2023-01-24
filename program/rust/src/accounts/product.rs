use solana_program::{entrypoint::ProgramResult, program_error::ProgramError, program_memory::sol_memcpy, account_info::AccountInfo};

use crate::{utils::{try_convert, pyth_assert}, deserialize::load_checked, instruction::CommandHeader};

use {
    super::{
        AccountHeader,
        PythAccount,
    },
    crate::c_oracle_header::{
        PC_ACCTYPE_PRODUCT,
        PC_PROD_ACC_SIZE,
    },
    bytemuck::{
        Pod,
        Zeroable,
    },
    solana_program::pubkey::Pubkey,
    std::mem::size_of,
};

#[repr(C)]
#[derive(Copy, Clone, Pod, Zeroable)]
pub struct ProductAccount {
    pub header:              AccountHeader,
    pub first_price_account: Pubkey,
}

impl PythAccount for ProductAccount {
    const ACCOUNT_TYPE: u32 = PC_ACCTYPE_PRODUCT;
    const INITIAL_SIZE: u32 = size_of::<ProductAccount>() as u32;
    const MINIMUM_SIZE: usize = PC_PROD_ACC_SIZE as usize;
}

pub fn update_product_metadata(instruction_data : &[u8], product_account : &AccountInfo, version : u32 ) -> ProgramResult {

    pyth_assert(
        instruction_data.len() >= size_of::<CommandHeader>(),
        ProgramError::InvalidInstructionData,
    )?;

    let new_data_len = instruction_data.len() - size_of::<CommandHeader>();
    let max_data_len = try_convert::<_, usize>(PC_PROD_ACC_SIZE)? - size_of::<ProductAccount>();
    pyth_assert(new_data_len <= max_data_len, ProgramError::InvalidArgument)?;
 
    let new_data = &instruction_data[size_of::<CommandHeader>()..instruction_data.len()];
    let mut idx = 0;
    // new_data must be a list of key-value pairs, both of which are instances of pc_str_t.
    // Try reading the key-value pairs to validate that new_data is properly formatted.
    while idx < new_data.len() {
        let key = read_pc_str_t(&new_data[idx..])?;
        idx += key.len();
        let value = read_pc_str_t(&new_data[idx..])?;
        idx += value.len();
    }

    // This assertion shouldn't ever fail, but be defensive.
    pyth_assert(idx == new_data.len(), ProgramError::InvalidArgument)?;

    {
        let mut data = product_account.try_borrow_mut_data()?;
        // Note that this memcpy doesn't necessarily overwrite all existing data in the account.
        // This case is handled by updating the .size_ field below.
        sol_memcpy(
            &mut data[size_of::<ProductAccount>()..],
            new_data,
            new_data.len(),
        );
    }

    let mut product_data = load_checked::<ProductAccount>(product_account, version)?;
    product_data.header.size = try_convert(size_of::<ProductAccount>() + new_data.len())?;
    Ok(())
}

/// Read a `pc_str_t` from the beginning of `source`. Returns a slice of `source` containing
/// the bytes of the `pc_str_t`.
pub fn read_pc_str_t(source: &[u8]) -> Result<&[u8], ProgramError> {
    if source.is_empty() {
        Err(ProgramError::InvalidArgument)
    } else {
        let tag_len: usize = try_convert(source[0])?;
        if tag_len + 1 > source.len() {
            Err(ProgramError::InvalidArgument)
        } else {
            Ok(&source[..(1 + tag_len)])
        }
    }
}