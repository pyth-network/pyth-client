use {
    super::pyth_simulator::PythSimulator,
    crate::{
        accounts::MappingAccount,
        error::OracleError,
    },
    solana_sdk::{
        instruction::InstructionError,
        signer::Signer,
        transaction::TransactionError,
    },
    std::mem::size_of,
};


#[tokio::test]
async fn test_resize_mapping() {
    let mut sim = PythSimulator::new().await;
    let mapping_keypair = sim.init_mapping().await.unwrap();
    assert_eq!(
        sim.resize_mapping(&mapping_keypair)
            .await
            .unwrap_err()
            .unwrap(),
        TransactionError::InstructionError(
            0,
            InstructionError::Custom(OracleError::NoNeedToResize as u32)
        )
    );

    sim.truncate_account(mapping_keypair.pubkey(), 20536).await; // Old size.
    for i in 0..14 {
        println!("resize #{i}");
        sim.resize_mapping(&mapping_keypair).await.unwrap();
    }
    assert_eq!(
        sim.resize_mapping(&mapping_keypair)
            .await
            .unwrap_err()
            .unwrap(),
        TransactionError::InstructionError(
            0,
            InstructionError::Custom(OracleError::NoNeedToResize as u32)
        )
    );
    assert_eq!(
        sim.get_account(mapping_keypair.pubkey())
            .await
            .unwrap()
            .data
            .len(),
        size_of::<MappingAccount>()
    );
}
