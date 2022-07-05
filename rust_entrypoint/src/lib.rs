use solana_program::{msg};


#[link(name = "legacyc")]
extern "C" {
    fn c_entrypoint(input: *mut u8) -> u64;
}

#[no_mangle]
pub unsafe extern "C" fn entrypoint(input: *mut u8) -> u64 {
    return c_entrypoint(input);
}