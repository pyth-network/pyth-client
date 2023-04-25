use {
    crate::{
        accounts::{
            AccountHeader,
            PermissionAccount,
            PERMISSIONS_SEED,
        },
        c_oracle_header::{
            MAX_CI_DIVISOR,
            MAX_NUM_DECIMALS,
            PC_STATUS_IGNORED,
        },
        deserialize::{
            load_account_as,
            load_checked,
        },
        instruction::{
            CommandHeader,
            OracleCommand,
            UpdPriceArgs,
        },
        OracleError,
    },
    bytemuck::{
        Pod,
        Zeroable,
    },
    num_traits::FromPrimitive,
    solana_program::{
        account_info::AccountInfo,
        bpf_loader_upgradeable,
        program_error::ProgramError,
        pubkey::Pubkey,
        sysvar::rent::Rent,
    },
    std::cell::Ref,
};


pub fn pyth_assert(condition: bool, error_code: ProgramError) -> Result<(), ProgramError> {
    if !condition {
        Result::Err(error_code)
    } else {
        Result::Ok(())
    }
}

pub fn valid_funding_account(account: &AccountInfo) -> bool {
    account.is_signer && account.is_writable
}

pub fn check_valid_funding_account(account: &AccountInfo) -> Result<(), ProgramError> {
    pyth_assert(
        valid_funding_account(account),
        OracleError::InvalidFundingAccount.into(),
    )
}

pub fn valid_signable_account(
    program_id: &Pubkey,
    account: &AccountInfo,
) -> Result<bool, ProgramError> {
    Ok(account.is_signer
        && account.is_writable
        && account.owner == program_id
        && get_rent()?.is_exempt(account.lamports(), account.data_len()))
}

pub fn check_valid_signable_account(
    program_id: &Pubkey,
    account: &AccountInfo,
) -> Result<(), ProgramError> {
    pyth_assert(
        valid_signable_account(program_id, account)?,
        OracleError::InvalidSignableAccount.into(),
    )
}

/// Check that `account` is a valid signable pyth account or
/// that `funding_account` is a signer and is permissioned by the `permission_account`
pub fn check_valid_signable_account_or_permissioned_funding_account(
    program_id: &Pubkey,
    account: &AccountInfo,
    funding_account: &AccountInfo,
    permissions_account_option: Option<&AccountInfo>,
    cmd_hdr: &CommandHeader,
) -> Result<(), ProgramError> {
    if let Some(permissions_account) = permissions_account_option {
        check_valid_permissions_account(program_id, permissions_account)?;
        let permissions_account_data =
            load_checked::<PermissionAccount>(permissions_account, cmd_hdr.version)?;
        check_valid_funding_account(funding_account)?;
        pyth_assert(
            permissions_account_data.is_authorized(
                funding_account.key,
                OracleCommand::from_i32(cmd_hdr.command)
                    .ok_or(OracleError::UnrecognizedInstruction)?,
            ),
            OracleError::PermissionViolation.into(),
        )?;
        check_valid_writable_account(program_id, account)
    } else {
        check_valid_signable_account(program_id, account)
    }
}

/// Returns `true` if the `account` is fresh, i.e., its data can be overwritten.
/// Use this check to prevent accidentally overwriting accounts whose data is already populated.
pub fn valid_fresh_account(account: &AccountInfo) -> bool {
    let pyth_acc = load_account_as::<AccountHeader>(account);
    match pyth_acc {
        Ok(pyth_acc) => pyth_acc.magic_number == 0 && pyth_acc.version == 0,
        Err(_) => false,
    }
}

pub fn check_valid_fresh_account(account: &AccountInfo) -> Result<(), ProgramError> {
    pyth_assert(
        valid_fresh_account(account),
        OracleError::InvalidFreshAccount.into(),
    )
}

// Check that an exponent is within the range of permitted exponents for price accounts.
pub fn check_exponent_range(expo: i32) -> Result<(), ProgramError> {
    pyth_assert(
        (-MAX_NUM_DECIMALS..=MAX_NUM_DECIMALS).contains(&expo),
        ProgramError::InvalidArgument,
    )
}

/// Convert `x: T` into a `U`, returning the appropriate `OracleError` if the conversion fails.
pub fn try_convert<T, U: TryFrom<T>>(x: T) -> Result<U, OracleError> {
    // Note: the error here assumes we're only applying this function to integers right now.
    U::try_from(x).map_err(|_| OracleError::IntegerCastingError)
}

fn valid_writable_account(
    program_id: &Pubkey,
    account: &AccountInfo,
) -> Result<bool, ProgramError> {
    Ok(account.is_writable
        && account.owner == program_id
        && get_rent()?.is_exempt(account.lamports(), account.data_len()))
}

pub fn check_valid_writable_account(
    program_id: &Pubkey,
    account: &AccountInfo,
) -> Result<(), ProgramError> {
    pyth_assert(
        valid_writable_account(program_id, account)?,
        OracleError::InvalidWritableAccount.into(),
    )
}


fn valid_readable_account(
    program_id: &Pubkey,
    account: &AccountInfo,
) -> Result<bool, ProgramError> {
    Ok(
        account.owner == program_id
            && get_rent()?.is_exempt(account.lamports(), account.data_len()),
    )
}

pub fn check_valid_readable_account(
    program_id: &Pubkey,
    account: &AccountInfo,
) -> Result<(), ProgramError> {
    pyth_assert(
        valid_readable_account(program_id, account)?,
        OracleError::InvalidReadableAccount.into(),
    )
}

pub fn check_valid_permissions_account(
    program_id: &Pubkey,
    account: &AccountInfo,
) -> Result<(), ProgramError> {
    check_valid_readable_account(program_id, account)?;
    let (permission_pda_address, _) =
        Pubkey::find_program_address(&[PERMISSIONS_SEED.as_bytes()], program_id);
    pyth_assert(
        permission_pda_address == *account.key,
        OracleError::InvalidPda.into(),
    )
}

/// Checks whether this instruction is trying to update an individual publisher's price (`true`) or
/// is only trying to refresh the aggregate (`false`)
pub fn is_component_update(cmd_args: &UpdPriceArgs) -> Result<bool, OracleError> {
    match OracleCommand::from_i32(cmd_args.header.command)
        .ok_or(OracleError::UnrecognizedInstruction)?
    {
        OracleCommand::UpdPrice | OracleCommand::UpdPriceNoFailOnError => Ok(true),
        _ => Ok(false),
    }
}

// Return PC_STATUS_IGNORED if confidence is bigger than price divided by MAX_CI_DIVISOR else returns status
pub fn get_status_for_update(price: i64, confidence: u64, status: u32) -> Result<u32, OracleError> {
    let mut threshold_conf = price / MAX_CI_DIVISOR;

    if threshold_conf < 0 {
        threshold_conf = -threshold_conf;
    }

    if confidence > try_convert::<_, u64>(threshold_conf)? {
        Ok(PC_STATUS_IGNORED)
    } else {
        Ok(status)
    }
}

/// This struct represents UpgradeableLoaderState from bpf-upgradable-loader.
/// Solana uses bincode for the struct. However the bincode crate is too big the space we have onchain,
/// therefore we will use bytemuck for deserialization
#[repr(C)]
#[derive(Pod, Zeroable, Copy, Clone)]
struct ProgramdataAccount {
    /// 3 is the variant for programdata
    pub account_type:          u32,
    /// Space for slot of last upgrade (we don't use this field)
    pub slot:                  [u32; 2],
    /// 0 if immutable, 1 if has upgrade authority
    pub has_upgrade_authority: u8,
    /// Upgrade authority of the program
    pub upgrade_authority:     Pubkey,
    /// Unused field needed for this struct to be Pod
    pub unused:                [u8; 3],
}

/// Check that `programdata_account` is actually the buffer for `program_id`.
/// Check that the authority in `programdata_account` matches `upgrade_authority_account`.
pub fn check_is_upgrade_authority_for_program(
    upgrade_authority_account: &AccountInfo,
    programdata_account: &AccountInfo,
    program_id: &Pubkey,
) -> Result<(), ProgramError> {
    let programdata_deserialized: Ref<ProgramdataAccount> =
        load_account_as::<ProgramdataAccount>(programdata_account)?;

    // 1. programdata_account is actually this program's buffer
    let (programdata_address, _) =
        Pubkey::find_program_address(&[&program_id.to_bytes()], &bpf_loader_upgradeable::id());

    pyth_assert(
        programdata_address.eq(programdata_account.key),
        OracleError::InvalidUpgradeAuthority.into(),
    )?;


    // 2. upgrade_authority_account is actually the authority inside programdata_account
    pyth_assert(
        programdata_deserialized
            .upgrade_authority
            .eq(upgrade_authority_account.key)
            && programdata_deserialized.account_type == 3
            && programdata_deserialized.has_upgrade_authority == 1,
        OracleError::InvalidUpgradeAuthority.into(),
    )?;

    Ok(())
}

#[cfg(not(test))]
pub fn get_rent() -> Result<Rent, ProgramError> {
    use solana_program::sysvar::Sysvar;
    Rent::get()
}

#[cfg(test)]
pub fn get_rent() -> Result<Rent, ProgramError> {
    Ok(Rent::default())
}
