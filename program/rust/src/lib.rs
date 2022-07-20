mod c_oracle_header;
mod time_machine_types;

//do not link with C during unit tests (which are built in native architecture, unlike libpyth.o)
#[cfg(target_arch = "bpf")]
#[link(name = "cpyth")]
extern "C" {
    fn c_entrypoint(input: *mut u8) -> u64;
}

//make the C entrypoint a no-op when running cargo test
#[cfg(not(target_arch = "bpf"))]
pub extern "C" fn c_entrypoint(input: *mut u8) -> u64 {
    0 //SUCCESS value
}

#[no_mangle]
pub extern "C" fn entrypoint(input: *mut u8) -> u64 {
    let c_ret_val = unsafe { c_entrypoint(input) };
    if c_ret_val == c_oracle_header::SUCCESSFULLY_UPDATED_AGGREGATE {
        //0 is the SUCCESS value for solana
        return 0;
    } else {
        return c_ret_val;
    }
}