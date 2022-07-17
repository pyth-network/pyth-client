pub mod time_machine_types;
pub mod c_oracle_header;
use solana_program::pubkey::Pubkey;
use solana_program::sysvar::slot_history::AccountInfo;
use std::mem;
use borsh::{BorshDeserialize, BorshSerialize};

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
        solana_program::log::sol_log("Please resize the account to allow for SMA tracking!");
        return unsafe{c_entrypoint(input)};
    }
    let c_ret_value = unsafe{c_entrypoint(input)};
    update_tracker();
    c_ret_value
}

pub fn update_ptice_account(program_id: &Pubkey, accounts: &Vec<AccountInfo>, instruction_data: &[u8]){
    let account_len = accounts[1].try_data_len().unwrap();
    let tracker_size = mem::size_of::<time_machine_types::TimeMachineWrapper>();
    let price_t_size =  mem::size_of::<c_oracle_header::pc_price_t>();
    if account_len == price_t_size{
        //compute the number of lamports needed in the price account to update it
        let rent = solana_program::rent::Rent{
            lamports_per_byte_year: solana_program::rent::DEFAULT_LAMPORTS_PER_BYTE_YEAR,
            exemption_threshold : solana_program::rent::DEFAULT_EXEMPTION_THRESHOLD,
            burn_percent : solana_program::rent::DEFAULT_BURN_PERCENT,
        };
        let lamports_needed: u64 = rent.minimum_balance(price_t_size + tracker_size).saturating_sub(accounts[1].lamports());
    
        //transfer lamports if nescissary
        if lamports_needed > 0{
            solana_program::system_instruction::transfer(accounts[0].key,
                accounts[1].key,
                lamports_needed);
        }
        //resize
        accounts[1].realloc(price_t_size + tracker_size, false).unwrap();
        initialize_tracker();
    }
}

pub fn update_version(program_id: &Pubkey, accounts: Vec<AccountInfo>, instruction_data: &[u8]) -> u64{

    //read account info
    let pyth_acc_info_size = mem::size_of::<c_oracle_header::pc_acc_t>();
    let mut pyth_acc_info = c_oracle_header::pc_acc_t::try_from_slice(&accounts[1].try_borrow_data().unwrap()[..pyth_acc_info_size]).unwrap();

    //check magic number
    if pyth_acc_info.magic_ != c_oracle_header::PC_MAGIC{
        panic!("incorrect magic number");
    }

    //update the accounts depending on type
    match pyth_acc_info.type_ {
        c_oracle_header::PC_ACCTYPE_PRICE => update_ptice_account(program_id, &accounts, instruction_data),
        _ => (),
    }

    //I do not think that we need to have PC_VERSION dependent logic now, but here seems like a good place to
    //do such a thing

    //update the account info
    pyth_acc_info.ver_ = c_oracle_header::PC_VERSION;
    //TODO: Should I update the size_ field? I am not sure how it is being used, or why it is needed. Is it account_size?
    pyth_acc_info.clone().serialize(& mut & mut accounts[1].data.borrow_mut()[..pyth_acc_info_size]);//.unwrap();
    0
}
