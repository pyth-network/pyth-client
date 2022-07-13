pub mod time_machine_types;

#[link(name = "cpyth")]
extern "C" {
    fn c_entrypoint(input: *mut u8) -> u64;
}

pub fn update_price(input: *mut u8) -> u64{
    solana_program::log::sol_log("I am updating a price");
    let c_ret_val = unsafe{c_entrypoint(input)} ;
    solana_program::log::sol_log("I have updated a price");
    c_ret_val

}

/*fn update_price(program_id: &Pubkey, accounts: &[AccountInfo], instruction_data: &[u8]) -> u64{

}*/