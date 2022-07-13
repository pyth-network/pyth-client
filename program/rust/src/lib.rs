mod time_machine_types;
#[link(name = "cpyth")]
extern "C" {
    fn c_entrypoint(input: *mut u8) -> u64;
}

#[no_mangle]
pub extern "C" fn entrypoint(input: *mut u8) -> u64 {
    unsafe {
        return c_entrypoint(input);
    }
}
