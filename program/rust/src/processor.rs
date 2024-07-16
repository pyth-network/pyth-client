use {
    crate::{
        error::OracleError,
        instruction::{
            load_command_header_checked,
            OracleCommand,
        },
    },
    solana_program::{
        entrypoint::ProgramResult,
        pubkey::Pubkey,
        sysvar::slot_history::AccountInfo,
    },
};

mod add_price;
mod add_product;
mod add_publisher;
mod del_price;
mod del_product;
mod del_publisher;
#[cfg(any(feature = "test", test))]
mod init_mapping;
mod init_price;
mod set_max_latency;
mod set_min_pub;
#[cfg(any(feature = "test", test))]
mod upd_permissions;
mod upd_price;
mod upd_product;

pub use {
    add_price::add_price,
    add_product::add_product,
    add_publisher::add_publisher,
    del_price::del_price,
    del_product::del_product,
    del_publisher::del_publisher,
    init_price::init_price,
    set_max_latency::set_max_latency,
    set_min_pub::set_min_pub,
    upd_price::{
        c_upd_aggregate,
        c_upd_twap,
        upd_price,
        upd_price_no_fail_on_error,
    },
    upd_product::upd_product,
};
#[cfg(any(feature = "test", test))]
pub use {
    init_mapping::init_mapping,
    upd_permissions::upd_permissions,
};

/// Dispatch to the right instruction in the oracle.
pub fn process_instruction(
    program_id: &Pubkey,
    accounts: &[AccountInfo],
    instruction_data: &[u8],
) -> ProgramResult {
    use OracleCommand::*;

    match load_command_header_checked(instruction_data)? {
        InitMapping => {
            #[cfg(any(feature = "test", test))]
            {
                init_mapping(program_id, accounts, instruction_data)
            }
            #[cfg(not(any(feature = "test", test)))]
            {
                Err(OracleError::UnrecognizedInstruction.into())
            }
        }
        AddMapping => Err(OracleError::UnrecognizedInstruction.into()),
        AddProduct => add_product(program_id, accounts, instruction_data),
        UpdProduct => upd_product(program_id, accounts, instruction_data),
        AddPrice => add_price(program_id, accounts, instruction_data),
        AddPublisher => add_publisher(program_id, accounts, instruction_data),
        DelPublisher => del_publisher(program_id, accounts, instruction_data),
        UpdPrice => upd_price(program_id, accounts, instruction_data),
        AggPrice => upd_price(program_id, accounts, instruction_data),
        InitPrice => init_price(program_id, accounts, instruction_data),
        InitTest => Err(OracleError::UnrecognizedInstruction.into()),
        UpdTest => Err(OracleError::UnrecognizedInstruction.into()),
        SetMinPub => set_min_pub(program_id, accounts, instruction_data),
        UpdPriceNoFailOnError => upd_price_no_fail_on_error(program_id, accounts, instruction_data),
        ResizePriceAccount => {
            solana_program::msg!("Oracle price resize instruction has been removed. Bailing out!");
            Err(OracleError::UnrecognizedInstruction.into())
        }
        DelPrice => del_price(program_id, accounts, instruction_data),
        DelProduct => del_product(program_id, accounts, instruction_data),
        UpdPermissions => {
            #[cfg(any(feature = "test", test))]
            {
                upd_permissions(program_id, accounts, instruction_data)
            }
            #[cfg(not(any(feature = "test", test)))]
            {
                Err(OracleError::UnrecognizedInstruction.into())
            }
        }
        SetMaxLatency => set_max_latency(program_id, accounts, instruction_data),
    }
}
