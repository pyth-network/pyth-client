use std::io::{
    Cursor,
    Read,
    Write,
};

use bridge::accounts::claim;
use bridge::{
    Claim,
    DeserializeGovernancePayload,
    DeserializePayload,
    PayloadMessage,
    SerializeGovernancePayload,
    SerializePayload,
    CHAIN_ID_SOLANA,
};
use byteorder::ReadBytesExt;
use solana_program::account_info::AccountInfo;
use solana_program::clock::Clock;
use solana_program::entrypoint::ProgramResult;
use solana_program::program::invoke_signed;
use solana_program::program_error::ProgramError;
use solana_program::pubkey::Pubkey;
use solana_program::rent::Rent;
use solitaire::{
    Derive,
    ExecutionContext,
    FromAccounts,
    Info,
    Mut,
    Peel,
    Signer,
    SolitaireError,
    Sysvar,
};

use crate::c_oracle_header::{
    PermissionAccount,
    PERMISSIONS_SEED,
    UPGRADE_AUTHORITY_SEED,
};
use crate::deserialize::{
    load,
    load_checked,
};
use crate::error::OracleError;
use crate::instruction::CommandHeader;
use crate::utils::{
    check_valid_funding_account,
    check_valid_permissions_account,
};

#[derive(FromAccounts)]
pub struct UpgradeContract<'b> {
    pub funding_account: Mut<Signer<Info<'b>>>,

    /// This account is checked by the Pyth checks
    pub permissions_account: Derive<Info<'b>, PERMISSIONS_SEED>,

    /// Pyth governance payload
    pub vaa: PayloadMessage<'b, PythGovernancePayloadUpgrade>,

    /// An Uninitialized Claim account to consume the VAA.
    pub claim: Mut<Claim<'b>>,

    /// All these accounts are passed down and checked by the BPF loader
    pub upgrade_authority: Derive<Info<'b>, UPGRADE_AUTHORITY_SEED>,
    pub new_buffer:        Mut<Info<'b>>,
    pub old_buffer:        Mut<Info<'b>>,
    pub own_address:       Mut<Info<'b>>,
    pub rent:              Sysvar<'b, Rent>,
    pub clock:             Sysvar<'b, Clock>,
    pub bpf_loader:        Info<'b>,
    pub system:            Info<'b>,
}


pub fn upg_program(
    program_id: &Pubkey,
    accounts: &[AccountInfo],
    instruction_data: &[u8],
) -> ProgramResult {
    let cmd_args = load::<CommandHeader>(instruction_data)?;

    // Solitaire checks
    let mut accs: Box<UpgradeContract> = FromAccounts::from(program_id, &mut accounts.iter(), &())?;
    // Solitaire context
    let context: ExecutionContext = ExecutionContext {
        program_id,
        accounts,
    };


    // Pyth checks
    check_valid_funding_account(&accs.funding_account)?;
    check_valid_permissions_account(program_id, &accs.permissions_account)?;
    let permissions_account_data =
        load_checked::<PermissionAccount>(&accs.permissions_account, cmd_args.version)?;

    // Verify source of governance
    verify_governance(&accs.vaa, &permissions_account_data)?;
    // Claim so no replayability
    claim::consume(
        &context,
        accs.funding_account.key,
        &mut accs.claim,
        &accs.vaa,
    )?;

    let (upgrade_authority_pda, bump_seed) =
        Pubkey::find_program_address(&[UPGRADE_AUTHORITY_SEED.as_bytes()], program_id);

    let upgrade_ix = solana_program::bpf_loader_upgradeable::upgrade(
        program_id,
        &accs.vaa.new_contract,
        &upgrade_authority_pda,
        accs.funding_account.key,
    );

    invoke_signed(
        &upgrade_ix,
        accounts,
        &[&[UPGRADE_AUTHORITY_SEED.as_bytes(), &[bump_seed]]],
    )?;

    Ok(())
}


/// Fail if the emitter is not the known governance key, or the emitting chain is not Solana.
fn verify_governance<T>(
    vaa: &PayloadMessage<T>,
    permissions_account_data: &PermissionAccount,
) -> ProgramResult
where
    T: DeserializePayload,
{
    let expected_emitter = permissions_account_data.master_authority;
    let current_emitter = Pubkey::new_from_array(vaa.meta().emitter_address);
    if expected_emitter != current_emitter || vaa.meta().emitter_chain != CHAIN_ID_SOLANA {
        Err(OracleError::InvalidGovernanceVaa.into())
    } else {
        Ok(())
    }
}

/// Payload for program upgrade
pub struct PythGovernancePayloadUpgrade {
    // Address of the new Implementation
    pub new_contract: Pubkey,
}

impl SerializeGovernancePayload for PythGovernancePayloadUpgrade {
    const MODULE: &'static str = "PythOracle";
    const ACTION: u8 = 0;
}

impl SerializePayload for PythGovernancePayloadUpgrade {
    fn serialize<W: Write>(&self, v: &mut W) -> std::result::Result<(), SolitaireError> {
        v.write_all(&self.new_contract.to_bytes())?;
        Ok(())
    }
}

impl DeserializePayload for PythGovernancePayloadUpgrade {
    fn deserialize(buf: &mut &[u8]) -> Result<Self, SolitaireError> {
        let mut c = Cursor::new(buf);
        let mut module = [0u8; 32];
        c.read_exact(&mut module)?;
        if module != format!("{:\0>32}", Self::MODULE).as_bytes() {
            return Err(ProgramError::InvalidAccountData.into());
        }

        let action = c.read_u8()?;
        if action != Self::ACTION {
            return Err(ProgramError::InvalidAccountData.into());
        }

        let chain = c.read_u16::<byteorder::BigEndian>()?;
        if chain != CHAIN_ID_SOLANA && chain != 0 {
            return Err(ProgramError::InvalidAccountData.into());
        }

        let mut addr = [0u8; 32];
        c.read_exact(&mut addr)?;

        if c.position() != c.into_inner().len() as u64 {
            return Err(ProgramError::InvalidAccountData.into());
        }

        Ok(PythGovernancePayloadUpgrade {
            new_contract: Pubkey::new(&addr[..]),
        })
    }
}

impl DeserializeGovernancePayload for PythGovernancePayloadUpgrade {
}
