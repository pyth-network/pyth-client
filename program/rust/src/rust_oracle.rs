mod time_machine_types;

#[link(name = "cpyth")]
extern "C" {
    fn c_entrypoint(input: *mut u8) -> u64;
}

pub fn update_price(input: *mut u8) -> u64{
    panic!("it's a trap")
}

/*fn update_price(program_id: &Pubkey, accounts: &[AccountInfo], instruction_data: &[u8]) -> u64{

}*/