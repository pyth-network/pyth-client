use {
    crate::{
        accounts::{
            AccountHeader,
            PermissionAccount,
            PERMISSIONS_SEED,
        },
        c_oracle_header::MAX_NUM_DECIMALS,
        deserialize::load_account_as,
        instruction::{
            CommandHeader,
            OracleCommand,
            UpdPriceArgs,
        },
        OracleError,
    },
    num_traits::FromPrimitive,
    solana_program::{
        account_info::AccountInfo,
        bpf_loader_upgradeable::UpgradeableLoaderState,
        program::invoke,
        program_error::ProgramError,
        pubkey::Pubkey,
        system_instruction::transfer,
        sysvar::rent::Rent,
    },
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

        let permissions_account_data = PermissionAccount::hardcoded();
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

/// Read a `pc_str_t` from the beginning of `source`. Returns a slice of `source` containing
/// the bytes of the `pc_str_t`.
pub fn read_pc_str_t(source: &[u8]) -> Result<&[u8], ProgramError> {
    if source.is_empty() {
        Err(ProgramError::InvalidArgument)
    } else {
        let tag_len: usize = try_convert(source[0])?;
        if tag_len + 1 > source.len() {
            Err(ProgramError::InvalidArgument)
        } else {
            Ok(&source[..(1 + tag_len)])
        }
    }
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


/// These 3 accounts need to get passed to make sure that the upgrade authority is signing the
/// transaction
/// - `program_account` is the program at address `program_id`. It just contains a pointer to the
///   `programdata_account`
/// - `programdata_account` has an `upgrade_authority_address` field that needs to match
///   `upgrade_authority.key`
#[allow(dead_code)]
pub fn check_is_upgrade_authority_for_program(
    upgrade_authority_account: &AccountInfo,
    program_account: &AccountInfo,
    programdata_account: &AccountInfo,
    program_id: &Pubkey,
) -> Result<(), ProgramError> {
    let program_deserialized: UpgradeableLoaderState =
        bincode::deserialize(&program_account.try_borrow_data()?)
            .map_err(|_| OracleError::DeserializationError)?;
    let programdata_deserialized: UpgradeableLoaderState =
        bincode::deserialize(&programdata_account.try_borrow_data()?)
            .map_err(|_| OracleError::DeserializationError)?;

    // 1. program_account is actually this program's account
    pyth_assert(
        program_account.key.eq(program_id) && program_account.executable,
        OracleError::InvalidUpgradeAuthority.into(),
    )?;

    // 2. programdata_account is actually this program's buffer
    if let UpgradeableLoaderState::Program {
        programdata_address,
    } = program_deserialized
    {
        pyth_assert(
            programdata_address.eq(programdata_account.key),
            OracleError::InvalidUpgradeAuthority.into(),
        )?;
    } else {
        return Err(OracleError::InvalidUpgradeAuthority.into());
    }

    // 3. upgrade_authority_account is actually the authority inside programdata_account
    if let UpgradeableLoaderState::ProgramData {
        slot: _,
        upgrade_authority_address: Some(upgrade_authority_key),
    } = programdata_deserialized
    {
        pyth_assert(
            upgrade_authority_key.eq(upgrade_authority_account.key),
            OracleError::InvalidUpgradeAuthority.into(),
        )?;
    } else {
        return Err(OracleError::InvalidUpgradeAuthority.into());
    }
    Ok(())
}

pub fn send_lamports<'a>(
    from: &AccountInfo<'a>,
    to: &AccountInfo<'a>,
    system_program: &AccountInfo<'a>,
    amount: u64,
) -> Result<(), ProgramError> {
    let transfer_instruction = transfer(from.key, to.key, amount);
    invoke(
        &transfer_instruction,
        &[from.clone(), to.clone(), system_program.clone()],
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
