pub mod time_machine_types;
pub mod c_oracle_header;
use solana_program::pubkey::Pubkey;
use solana_program::sysvar::slot_history::AccountInfo;
use std::mem;

#[link(name = "cpyth")]
extern "C" {
    fn c_entrypoint(input: *mut u8) -> u64;
}

pub fn initialize_tracker(){
    solana_program::log::sol_log("tracker initialized!");

    //TODO:connect to tracker struct
}

pub fn update_tracker(){
    solana_program::log::sol_log("tracker updated!");
    
    //TODO:connect to tracker struct

}

pub fn update_price(program_id: &Pubkey, accounts: Vec<AccountInfo>, instruction_data: &[u8], input: *mut u8) -> u64{
    
    let account_len = accounts[1].try_data_len().unwrap();
    let tracker_size = mem::size_of::<time_machine_types::TimeMachineWrapper>();
    let price_t_size =  mem::size_of::<c_oracle_header::pc_price_t>();

    if account_len < price_t_size + tracker_size  { 
        assert_eq!(account_len, price_t_size);
        let required_lamports = 10000000000 ;
        if  accounts[1].try_borrow_lamports().unwrap().clone() > required_lamports {
            solana_program::log::sol_log("We resized the account!");
            //since we are increasing the size for the first time it should be zero initialized by default
            accounts[1].realloc(price_t_size + tracker_size, false).unwrap();
            initialize_tracker();
        } else{
            solana_program::log::sol_log("Not enough lamports to resize");
        }
        return unsafe{c_entrypoint(input)};
    }
    update_tracker();
    unsafe{c_entrypoint(input)}
}
