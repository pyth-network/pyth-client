use num_derive::FromPrimitive;
#[repr(i32)]
#[derive(FromPrimitive)]
pub enum OracleCommand {
    // initialize first mapping list account
    // account[0] funding account       [signer writable]
    // account[1] mapping account       [signer writable]
    InitMapping,
    // initialize and add new mapping account
    // account[0] funding account       [signer writable]
    // account[1] tail mapping account  [signer writable]
    // account[2] new mapping account   [signer writable]
    AddMapping,
    // initialize and add new product reference data account
    // account[0] funding account       [signer writable]
    // account[1] mapping account       [signer writable]
    // account[2] new product account   [signer writable]
    AddProduct,
    // update product account
    // account[0] funding account       [signer writable]
    // account[1] product account       [signer writable]
    UpdProduct,
    // add new price account to a product account
    // account[0] funding account       [signer writable]
    // account[1] product account       [signer writable]
    // account[2] new price account     [signer writable]
    AddPrice,
    // add publisher to symbol account
    // account[0] funding account       [signer writable]
    // account[1] price account         [signer writable]
    AddPublisher,
    // delete publisher from symbol account
    // account[0] funding account       [signer writable]
    // account[1] price account         [signer writable]
    DelPublisher,
    // publish component price
    // account[0] funding account       [signer writable]
    // account[1] price account         [writable]
    // account[2] sysvar_clock account  []
    UpdPrice,
    // compute aggregate price
    // account[0] funding account       [signer writable]
    // account[1] price account         [writable]
    // account[2] sysvar_clock account  []
    AggPrice,
    // (re)initialize price account
    // account[0] funding account       [signer writable]
    // account[1] new price account     [signer writable]
    InitPrice,
    // deprecated
    InitTest,
    // deprecated
    UpdTest,
    // set min publishers
    // account[0] funding account       [signer writable]
    // account[1] price account         [signer writable]
    SetMinPub,
    // publish component price, never returning an error even if the update failed
    // account[0] funding account       [signer writable]
    // account[1] price account         [writable]
    // account[2] sysvar_clock account  []
    UpdPriceNoFailOnError,
    // resizes a price account so that it fits the Time Machine
    // account[0] funding account       [signer writable]
    // account[1] price account         [signer writable]
    // account[2] system program        []
    ResizePriceAccount,
    // deletes a price account
    // account[0] funding account       [signer writable]
    // account[1] product account       [signer writable]
    // account[2] price account         [signer writable]
    DelPrice
}