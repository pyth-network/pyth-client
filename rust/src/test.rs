use crate::{
  client::{
    Client,
    MappingAccount,
  },
};

use std::{
  borrow::Borrow,
  collections::{HashMap, HashSet},
  fmt::Debug,
  hash::Hash,
  str::FromStr,
};

use solana_sdk::{
  pubkey::Pubkey,
};

use solana_client::{
  rpc_client::RpcClient,
};

pub struct TestProduct {
  pub symbol: String,
  pub key: Pubkey,
  pub price: Pubkey,
}

pub struct Test {
  pub client: Box<dyn Client>,
  pub program: Pubkey,
  pub mapping: Pubkey,
  pub products: Vec<TestProduct>,
}

pub fn map_attrs<T, K: Eq + Hash + Clone + Debug, V: Clone + Debug>(
  objs: impl IntoIterator<Item = T>,
  f: impl Fn(T) -> (K, V),
) -> HashMap<K, V> {
  let mut res: HashMap<K, V> = Default::default();

  for obj in objs.into_iter() {
    let (key, val) = f(obj);
    let old = res.insert(key.clone(), val.clone());
    assert!(
      old.is_none(),
      "Key '{:?}' is mapped to both '{:?}' and '{:?}'.",
      key,
      val,
      old.unwrap(),
    );
  }

  res
}

pub fn set_diff<T: Eq + Hash>(
  base: impl IntoIterator<Item = T>,
  sub: impl IntoIterator<Item = T>,
) -> Vec<T> {
  let base: HashSet<T> = base.into_iter().collect();
  sub.into_iter().filter(|x| {
    ! base.contains(x)
  }).collect()
}

impl Test {

  pub fn expected_product_keys(&self) -> Vec<Pubkey> {
    self.products.iter().map(|p| p.key).collect()
  }

  pub fn expected_product_prices(&self) -> HashMap<Pubkey, Pubkey> {
    map_attrs(&self.products, |p| (p.key, p.price))
  }

  pub fn expected_product_symbols(&self) -> HashMap<Pubkey, String> {
    map_attrs(&self.products, |p| (p.key, p.symbol.clone()))
  }

  fn load_mappings(&self) -> Vec<MappingAccount> {
    let mappings = MappingAccount::load_all(
      self.client.borrow(),
      &self.mapping,
    ).unwrap();

    let root = mappings.get(0).unwrap();
    assert_eq!(&root.meta.key, &self.mapping);

    mappings
  }

  pub fn run(&self) -> Vec<MappingAccount> {
    let exp_prod_keys = self.expected_product_keys();
    let exp_prod_prices = self.expected_product_prices();
    let exp_prod_symbols = self.expected_product_symbols();
    let mut found_prod_keys: HashSet<Pubkey> = Default::default();

    let mapping_accounts = self.load_mappings();
    for mapping_acc in &mapping_accounts {
      assert_eq!(&mapping_acc.meta.owner, &self.program);

      for prod_acc in &mapping_acc.products {
        let prod_key = &prod_acc.meta.key;
        let prod_is_new = found_prod_keys.insert(*prod_key);
        assert!(prod_is_new, "Duplicate product key: {:?}", prod_key);

        if let Some(symbol) = exp_prod_symbols.get(prod_key) {
          assert_eq!(symbol, prod_acc.symbol());
        }
        if let Some(price_key) = exp_prod_prices.get(prod_key) {
          assert_eq!(price_key, prod_acc.data.first_price());
        }

        assert_eq!(&prod_acc.meta.owner, &self.program);
        for price_acc in &prod_acc.prices {
          assert_eq!(&price_acc.meta.owner, &self.program);
        }
      }

    }

    let missing = set_diff(found_prod_keys, exp_prod_keys);
    assert!(missing.is_empty(), "Missing product keys: {:?}", missing);

    mapping_accounts
  }

}

#[test]
fn mainnet() {
  Test{

    client: Box::new(RpcClient::new(
      "https://api.mainnet-beta.solana.com".into()
    )),

    // https://github.com/pyth-network/pyth-client-js/blob/main/src/cluster.ts
    program: Pubkey::from_str(
      "FsJ3A3u2vn5cTVofAjvy6y5kwABJAqYWpe4975bi2epH"
    ).unwrap(),

    // https://pyth.network/developers/accounts
    mapping: Pubkey::from_str(
      "AHtgzX45WTKfkPG53L6WYhGEXwQkN1BVknET3sVsLL8J"
    ).unwrap(),

    products: vec![
      TestProduct {
        symbol: String::from(
          "BTC/USD"
        ),
        key: Pubkey::from_str(
          "4aDoSXJ5o3AuvL7QFeR6h44jALQfTmUUCTVGDD6aoJTM"
        ).unwrap(),
        price: Pubkey::from_str(
          "GVXRSBjFk6e6J3NbVPXohDJetcTjaeeuykUpbQF8UoMU"
        ).unwrap(),
      },
    ],

  }.run();
}
