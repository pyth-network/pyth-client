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
