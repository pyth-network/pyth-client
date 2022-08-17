use crate::c_oracle_header::{
    pc_map_table_t,
    pc_price_comp_t,
    pc_price_info_t,
    pc_price_t,
    pc_pub_key_t,
    PC_COMP_SIZE,
    PC_MAP_TABLE_SIZE,
};
use std::mem::size_of;

#[test]
fn test_sizes() {
    assert_eq!(size_of::<pc_pub_key_t>(), 32);
    assert_eq!(
        size_of::<pc_map_table_t>(),
        24 + (PC_MAP_TABLE_SIZE as usize + 1) * size_of::<pc_pub_key_t>()
    );
    assert_eq!(size_of::<pc_price_info_t>(), 32);
    assert_eq!(
        size_of::<pc_price_comp_t>(),
        size_of::<pc_pub_key_t>() + 2 * size_of::<pc_price_info_t>()
    );
    assert_eq!(
        size_of::<pc_price_t>(),
        48 + 8 * size_of::<u64>()
            + 3 * size_of::<pc_pub_key_t>()
            + size_of::<pc_price_info_t>()
            + (PC_COMP_SIZE as usize) * size_of::<pc_price_comp_t>()
    );
}
