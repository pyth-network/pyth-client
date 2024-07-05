use {
    crate::{
        accounts::{
            AccountHeader,
            PriceAccount,
            PythAccount,
        },
        c_oracle_header::PC_MAGIC,
        error::OracleError,
        utils::pyth_assert,
    },
    solana_sdk::program_error::ProgramError,
    std::mem::size_of,
};

pub fn accumulate_price(price_account_info: &mut [u8]) -> Result<(), ProgramError> {
    // TODO: don't return error on non-price account?
    pyth_assert(
        price_account_info.len() >= PriceAccount::MINIMUM_SIZE,
        OracleError::AccountTooSmall.into(),
    )?;

    {
        let account_header = bytemuck::from_bytes::<AccountHeader>(
            &price_account_info[0..size_of::<AccountHeader>()],
        );

        pyth_assert(
            account_header.magic_number == PC_MAGIC
                && account_header.account_type == PriceAccount::ACCOUNT_TYPE,
            OracleError::InvalidAccountHeader.into(),
        )?;
    }

    let _price_info = bytemuck::from_bytes_mut::<PriceAccount>(
        &mut price_account_info[0..size_of::<PriceAccount>()],
    );

    todo!()
}
