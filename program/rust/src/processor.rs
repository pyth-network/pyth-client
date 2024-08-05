use {
    crate::{
        accounts::{
            AccountHeader,
            PermissionAccount,
        },
        deserialize::load_account_as_mut,
        error::OracleError,
        instruction::{
            load_command_header_checked,
            OracleCommand,
        },
        utils::{
            pyth_assert,
            try_convert,
        },
    },
    solana_program::{
        entrypoint::ProgramResult,
        program::invoke,
        pubkey::Pubkey,
        system_instruction,
        sysvar::slot_history::AccountInfo,
    },
};

mod add_price;
mod add_product;
mod add_publisher;
mod del_price;
mod del_product;
mod del_publisher;
mod init_mapping;
mod init_price;
mod init_price_feed_index;
mod set_max_latency;
mod set_min_pub;
mod upd_permissions;
mod upd_price;
mod upd_product;

#[cfg(test)]
pub use add_publisher::{
    DISABLE_ACCUMULATOR_V2,
    ENABLE_ACCUMULATOR_V2,
};
pub use {
    add_price::add_price,
    add_product::add_product,
    add_publisher::add_publisher,
    del_price::del_price,
    del_product::del_product,
    del_publisher::del_publisher,
    init_mapping::init_mapping,
    init_price::init_price,
    set_max_latency::set_max_latency,
    set_min_pub::set_min_pub,
    upd_permissions::upd_permissions,
    upd_price::{
        c_upd_aggregate,
        c_upd_twap,
        find_publisher_index,
        upd_price,
        upd_price_no_fail_on_error,
    },
    upd_product::upd_product,
};
use {
    init_price_feed_index::init_price_feed_index,
    solana_program::{
        program_error::ProgramError,
        rent::Rent,
        sysvar::Sysvar,
    },
};

/// Dispatch to the right instruction in the oracle.
pub fn process_instruction(
    program_id: &Pubkey,
    accounts: &[AccountInfo],
    instruction_data: &[u8],
) -> ProgramResult {
    use OracleCommand::*;

    match load_command_header_checked(instruction_data)? {
        InitMapping => init_mapping(program_id, accounts, instruction_data),
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
        UpdPermissions => upd_permissions(program_id, accounts, instruction_data),
        SetMaxLatency => set_max_latency(program_id, accounts, instruction_data),
        InitPriceFeedIndex => init_price_feed_index(program_id, accounts, instruction_data),
    }
}

fn reserve_new_price_feed_index<'a>(
    funding_account: &AccountInfo<'a>,
    permissions_account: &AccountInfo<'a>,
    system_program: &AccountInfo<'a>,
) -> Result<u32, ProgramError> {
    if permissions_account.data_len() < PermissionAccount::MIN_SIZE_WITH_LAST_FEED_INDEX {
        let new_size = PermissionAccount::MIN_SIZE_WITH_LAST_FEED_INDEX;
        let rent = Rent::get()?;
        let new_minimum_balance = rent.minimum_balance(new_size);
        let lamports_diff = new_minimum_balance.saturating_sub(permissions_account.lamports());
        if lamports_diff > 0 {
            invoke(
                &system_instruction::transfer(
                    funding_account.key,
                    permissions_account.key,
                    lamports_diff,
                ),
                &[
                    funding_account.clone(),
                    permissions_account.clone(),
                    system_program.clone(),
                ],
            )?;
        }

        permissions_account.realloc(new_size, true)?;
        let mut header = load_account_as_mut::<AccountHeader>(permissions_account)?;
        header.size = try_convert(new_size)?;
    }
    let mut last_feed_index = PermissionAccount::load_last_feed_index_mut(permissions_account)?;
    *last_feed_index += 1;
    pyth_assert(
        *last_feed_index < (1 << 28),
        OracleError::MaxLastFeedIndexReached.into(),
    )?;
    Ok(*last_feed_index)
}
