use crate::{
  data::{
    AccountData,
    Price,
    Product,
    Mapping,
  },
  error::{
    BoxResult,
    PythError,
    PythResult,
  },
  util::{
    debug::DebugResult,
  },
};

use std::{
  collections::HashMap,
};

use serde::{
  Serialize,
  Deserialize,
};

use solana_sdk::{
  account::Account,
  clock::Epoch,
  pubkey::Pubkey,
};

use solana_client::{
  rpc_client::RpcClient,
};

pub trait Client {
  fn get_account(&self, pubkey: &Pubkey) -> BoxResult<Account>;
}

impl Client for RpcClient {
  fn get_account(&self, pubkey: &Pubkey) -> BoxResult<Account> {
    Ok(RpcClient::get_account(self, pubkey)?)
  }
}

#[derive(Debug, Default, Clone, Serialize, Deserialize)]
pub struct AccountMeta {
  pub key: Pubkey,
  pub lamports: u64,
  pub owner: Pubkey,
  pub executable: bool,
  pub rent_epoch: Epoch,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct PriceAccount {
  pub meta: AccountMeta,
  pub data: Price,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ProductAccount {
  pub meta: AccountMeta,
  pub data: Product,
  pub prices: Vec<PriceAccount>,
  pub attrs: HashMap<String, String>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct MappingAccount {
  pub meta: AccountMeta,
  pub data: Mapping,
  pub products: Vec<ProductAccount>,
}

impl AccountMeta {

  pub fn from_account(acc: &Account, key: &Pubkey) -> Self {
    Self{
      key: *key,
      lamports: acc.lamports,
      owner: acc.owner,
      executable: acc.executable,
      rent_epoch: acc.rent_epoch,
    }
  }

}

impl PriceAccount {

  pub fn from_account(acc: &Account, key: &Pubkey) -> PythResult<Self> {
    Ok(Self{
      meta: AccountMeta::from_account(acc, key),
      data: *Price::from_account(acc)?,
    })
  }

  pub fn load(client: &dyn Client, key: &Pubkey) -> BoxResult<Self> {
    let acc = client.get_account(key).debug()?;
    Ok(Self::from_account(&acc, key)?)
  }

}

impl ProductAccount {

  const ATTR_SYMBOL: &'static str = "symbol";

  pub fn from_account(acc: &Account, key: &Pubkey) -> PythResult<Self> {
    let data = Product::from_account(acc)?;
    let res = Self{
      meta: AccountMeta::from_account(acc, key),
      data: *data,
      prices: Default::default(),
      attrs: unsafe { data.attr_map()? },
    };
    res.attr(Self::ATTR_SYMBOL)?;
    Ok(res)
  }

  pub fn attr(&self, key: &str) -> PythResult<&String> {
    self.attrs.get(key).ok_or_else(|| {
      PythError::MissingAttr{ attr: key.into() }.debug()
    })
  }

  pub fn symbol(&self) -> &str {
    self.attrs.get(Self::ATTR_SYMBOL).unwrap()
  }

  pub fn load(client: &dyn Client, product_key: &Pubkey) -> BoxResult<Self> {

    let acc = client.get_account(product_key).debug()?;
    let mut res = Self::from_account(&acc, product_key)?;

    let load_price = |key: &Pubkey| {
      PriceAccount::load(client, key)
    };

    res.prices.push(
      load_price(res.data.first_price())?,
    );

    while let Some(next_key) = {
      res.prices.last().unwrap().data.next_price().cloned()
    } {
      res.prices.push(load_price(&next_key)?);
    }

    Ok(res)
  }

}

impl MappingAccount {

  pub fn from_account(acc: &Account, key: &Pubkey) -> PythResult<Self> {
    Ok(Self{
      meta: AccountMeta::from_account(acc, key),
      data: *Mapping::from_account(acc)?,
      products: Default::default(),
    })
  }

  pub fn load(client: &dyn Client, mapping_key: &Pubkey) -> BoxResult<Self> {

    let acc = client.get_account(mapping_key).debug()?;
    let mut res = Self::from_account(&acc, mapping_key)?;

    for product_key in res.data.product_keys() {
      res.products.push(
        ProductAccount::load(client, product_key)?
      );
    }

    Ok(res)
  }

  /// Load all pyth accounts given a root mapping key, e.g., from
  /// https://pyth.network/developers/accounts
  pub fn load_all(
    client: &dyn Client,
    root_key: &Pubkey,
  ) -> BoxResult<Vec<Self>> {

    let mut res = vec![
      Self::load(client, &root_key)?,
    ];

    while let Some(key) = res.last().unwrap().data.next_mapping().cloned() {
      res.push(
        Self::load(client, &key)?
      );
    }

    Ok(res)
  }

}
