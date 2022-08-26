use crate::error::OracleError;
use crate::instruction::{
    load_command_header_checked,
    OracleCommand,
};
use solana_program::entrypoint::ProgramResult;
use solana_program::pubkey::Pubkey;
use solana_program::sysvar::slot_history::AccountInfo;

use crate::rust_oracle::{
    add_mapping,
    add_price,
    add_product,
    add_publisher,
    del_price,
    del_product,
    del_publisher,
    init_mapping,
    init_price,
    resize_price_account,
    set_min_pub,
    upd_price,
    upd_price_no_fail_on_error,
    upd_product, upd_authorities,
};

///dispatch to the right instruction in the oracle
pub fn process_instruction(
    program_id: &Pubkey,
    accounts: &[AccountInfo],
    instruction_data: &[u8],
) -> ProgramResult {
    match load_command_header_checked(instruction_data)? {
        OracleCommand::InitMapping => init_mapping(program_id, accounts, instruction_data),
        OracleCommand::AddMapping => add_mapping(program_id, accounts, instruction_data),
        OracleCommand::AddProduct => add_product(program_id, accounts, instruction_data),
        OracleCommand::UpdProduct => upd_product(program_id, accounts, instruction_data),
        OracleCommand::AddPrice => add_price(program_id, accounts, instruction_data),
        OracleCommand::AddPublisher => add_publisher(program_id, accounts, instruction_data),
        OracleCommand::DelPublisher => del_publisher(program_id, accounts, instruction_data),
        OracleCommand::UpdPrice => upd_price(program_id, accounts, instruction_data),
        OracleCommand::AggPrice => upd_price(program_id, accounts, instruction_data),
        OracleCommand::InitPrice => init_price(program_id, accounts, instruction_data),
        OracleCommand::InitTest => Err(OracleError::UnrecognizedInstruction.into()),
        OracleCommand::UpdTest => Err(OracleError::UnrecognizedInstruction.into()),
        OracleCommand::SetMinPub => set_min_pub(program_id, accounts, instruction_data),
        OracleCommand::UpdPriceNoFailOnError => {
            upd_price_no_fail_on_error(program_id, accounts, instruction_data)
        }
        OracleCommand::ResizePriceAccount => {
            resize_price_account(program_id, accounts, instruction_data)
        }
        OracleCommand::DelPrice => del_price(program_id, accounts, instruction_data),
        OracleCommand::DelProduct => del_product(program_id, accounts, instruction_data),
        OracleCommand::UpdAuthorities => upd_authorities(program_id, accounts, instruction_data)
    }
}
