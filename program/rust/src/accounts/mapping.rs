use {
    super::{
        AccountHeader,
        PythAccount,
    },
    crate::c_oracle_header::{
        PC_ACCTYPE_MAPPING,
        PC_MAP_TABLE_SIZE,
        PC_MAP_TABLE_T_PROD_OFFSET,
    },
    bytemuck::{
        Pod,
        Zeroable,
    },
    solana_program::pubkey::Pubkey,
};

#[repr(C)]
#[derive(Copy, Clone)]
pub struct MappingAccount {
    pub header:               AccountHeader,
    pub number_of_products:   u32,
    pub unused_:              u32,
    pub next_mapping_account: Pubkey,
    pub products_list:        [Pubkey; PC_MAP_TABLE_SIZE as usize],
}

impl PythAccount for MappingAccount {
    const ACCOUNT_TYPE: u32 = PC_ACCTYPE_MAPPING;
    /// Equal to the offset of `prod_` in `MappingAccount`, see the trait comment for more detail
    const INITIAL_SIZE: u32 = PC_MAP_TABLE_T_PROD_OFFSET as u32;
}

// Unsafe impl because product_list is of size 640 and there's no derived trait for this size
unsafe impl Pod for MappingAccount {
}

unsafe impl Zeroable for MappingAccount {
}
