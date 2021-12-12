use crate::{
  bindings as c,
  error::{
    PythError,
    PythResult,
    check_field_eq,
    check_field_ge,
    check_field_gt,
    check_field_le,
    check_field_in,
  },
  util::{
    sa::const_assert,
    bytes::ByteCast,
    valid::Valid,
  },
};

use std::{
  collections::HashMap,
  slice,
};

use solana_sdk::{
  account::Account,
  pubkey::Pubkey,
};

pub use c::{
  PC_VERSION as VERSION,
  PC_MAGIC as MAGIC,
  pc_acc_t as AccountHeader,
  pc_price_t as Price,
  pc_price_info_t as PriceInfo,
  pc_price_comp_t as PriceComponent,
  pc_prod_t as Product,
  pc_map_table_t as Mapping,
  pc_ema_t as Ema,
};

impl PriceInfo {
  pub const STATUS_UNKNOWN: u32 = c::PC_STATUS_UNKNOWN;
  pub const STATUS_TRADING: u32 = c::PC_STATUS_TRADING;
  pub const STATUS_HALTED: u32 = c::PC_STATUS_HALTED;
  pub const STATUS_AUCTION: u32 = c::PC_STATUS_AUCTION;
}

impl Valid for PriceInfo {
  type Error = PythError;

  fn validate(&self) -> PythResult<()> {
    check_field_in!(self, status_, [
      Self::STATUS_UNKNOWN,
      Self::STATUS_TRADING,
      Self::STATUS_HALTED,
      Self::STATUS_AUCTION,
    ])
  }
}

impl Valid for PriceComponent {
  type Error = PythError;

  fn validate(&self) -> PythResult<()> {
    Valid::validate(&self.agg_)?;
    Valid::validate(&self.latest_)?;
    self.pub_.err_if_zero(|| format!("price component publisher"))
  }
}

pub trait ValidAccount: Sized {
  /// Min value of [`AccountHeader::size_`].
  const MIN_SIZE: usize;

  /// Expected value of [`AccountHeader::size_`].
  fn expected_size(&self) -> PythResult<usize>;

  /// Validate the data segment after [`AccountHeader`].
  fn validate_payload(&self) -> PythResult<()>;
}

/// A [`Mapping`], [`Product`], or [`Price`] decoded from [`Account::data`]`.
pub trait AccountData: ValidAccount {

  const TYPE: u32;

  fn from_account(acc: &Account) -> PythResult<&Self>;

  fn header(&self) -> &AccountHeader {
    unsafe {
      as_type!(AccountHeader, self)
    }
  }

  fn size(&self) -> usize {
    self.header().size_ as usize
  }

  fn validate(&self) -> PythResult<()> {
    let h = self.header();
    check_field_eq!(h, ver_, VERSION)?;
    check_field_eq!(h, magic_, MAGIC)?;
    check_field_ge!(h, size_, Self::MIN_SIZE as u32)?;
    check_field_eq!(h, size_, self.expected_size()? as u32)?;
    self.validate_payload()
  }
}

// Requires implementing ValidAccount:
macro_rules! impl_pyth_account {
  ($T: ty, $acctype: expr) => {

    impl AccountData for $T {
      const TYPE: u32 = $acctype;
      fn from_account(acc: &Account) -> PythResult<&Self> {
        let mut buf = acc.data.as_slice();
        Ok(Self::from_borsh(&mut buf)?)
      }
    }

    impl Valid for $T {
      type Error = PythError;
      fn validate(&self) -> PythResult<()> {
        <Self as AccountData>::validate(self)
      }
    }

    impl_byte_cast!(
      $T,
      AccountData::size,
      <$T as ValidAccount>::MIN_SIZE
    );

  };
}

impl Price {

  pub const MAX_COMPONENTS: usize = c::PC_COMP_SIZE as usize;

  pub fn next_price(&self) -> Option<&Pubkey> {
    self.next_.if_nonzero()
  }

  pub fn component_count(&self) -> usize {
    self.num_ as usize
  }

  pub fn components(&self) -> &[PriceComponent] {
    &self.comp_[..self.component_count()]
  }

}

const MAX_COMPS_SIZE: usize = sizeof!(PriceComponent) * Price::MAX_COMPONENTS;
const MAX_PRICE_SIZE: usize = sizeof!(Price);
const_assert!(MAX_PRICE_SIZE > MAX_COMPS_SIZE);
const MIN_PRICE_SIZE: usize = MAX_PRICE_SIZE - MAX_COMPS_SIZE;

impl ValidAccount for Price {

  const MIN_SIZE: usize = MIN_PRICE_SIZE;

  fn expected_size(&self) -> PythResult<usize> {
    Ok(Self::MIN_SIZE + sizeof!(PriceComponent) * self.component_count())
  }

  fn validate_payload(&self) -> PythResult<()> {
    check_field_eq!(self, ptype_, c::PC_PTYPE_PRICE)?;
    check_field_le!(self, num_, Self::MAX_COMPONENTS as u32)?;
    for component in self.components() {
      component.validate()?;
    }
    self.prod_.err_if_zero(|| format!("price account product"))
  }

}

impl Product {

  pub fn first_price(&self) -> &Pubkey {
    &self.px_acc_
  }

  unsafe fn attr_data(&self) -> &[u8] {
    let len = self.size_ as usize - sizeof!(Self);
    let ptr: *const u8 = (self as *const Self).add(1).cast();
    slice::from_raw_parts(ptr, len)
  }

  fn get_attr_str(buf: &[u8]) -> PythResult<&str> {
    match std::str::from_utf8(buf) {
      Ok(val) => Ok(val),
      Err(err) => Err(err.into()),
    }
  }

  unsafe fn get_attr_strs<'a>(
    &'a self,
    mut f: impl FnMut(&'a str) -> PythResult<()>,
  ) -> PythResult<()> {

    let data = self.attr_data();
    let mut i: usize = 0;

    while i < data.len() {
      let len: u8 = data[i];
      let start = i + 1;
      let end = start + len as usize;
      f(Self::get_attr_str(&data[start..end])?)?;
      i = end;
    }

    Ok(())
  }

  pub unsafe fn attr_pairs(&self) -> PythResult<Vec<(&str, &str)>> {
    let mut strs: Vec<&str> = vec![];
    self.get_attr_strs(|s: &str| Ok(strs.push(s.into())))?;

    if strs.len() % 2 != 0 {
      return Err(PythError::UnpairedAttr{}.debug());
    }

    let mut pairs: Vec<(&str, &str)> = vec![];
    for i in 0..strs.len() / 2 {
      pairs.push((
        strs[i * 2],
        strs[i * 2 + 1],
      ));
    }

    Ok(pairs)
  }

  pub unsafe fn attr_map(&self) -> PythResult<HashMap<String, String>> {
    let mut map: HashMap<String, String> = Default::default();
    for (k, v) in self.attr_pairs()? {
      if let Some(_) = map.insert(k.into(), v.into()) {
        return Err(PythError::DuplicateAttr{ attr: k.into() }.debug());
      }
    };
    Ok(map)
  }

}

impl ValidAccount for Product {

  const MIN_SIZE: usize = sizeof!(Product);

  fn expected_size(&self) -> PythResult<usize> {
    let mut size = Self::MIN_SIZE;
    unsafe {
      self.get_attr_strs(|s: &str| {
        Ok(size += s.len() + 1) // +1 for length encoded as u8
      })?;
    }
    Ok(size)
  }

  fn validate_payload(&self) -> PythResult<()> {
    self.first_price().err_if_zero(|| format!("product first price"))
  }

}

impl Mapping {

  const MAX_PRODUCTS: usize = c::PC_MAP_TABLE_SIZE as usize;

  pub fn next_mapping(&self) -> Option<&Pubkey> {
    self.next_.if_nonzero()
  }

  pub fn product_count(&self) -> usize {
    self.num_ as usize
  }

  pub fn product_keys(&self) -> impl Iterator<Item = &Pubkey> {
    self.raw_product_keys().iter().filter(|key: &&Pubkey| {
      key.is_nonzero()
    })
  }

  // May contain zeroed keys.
  pub fn raw_product_keys(&self) -> &[Pubkey] {
    &self.prod_[..self.product_count()]
  }

}

const MAX_PRODUCTS_SIZE: usize = sizeof!(Pubkey) * Mapping::MAX_PRODUCTS;
const MAX_MAPPING_SIZE: usize = sizeof!(Mapping);
const_assert!(MAX_MAPPING_SIZE > MAX_PRODUCTS_SIZE);
const MIN_MAPPING_SIZE: usize = MAX_MAPPING_SIZE - MAX_PRODUCTS_SIZE;

impl ValidAccount for Mapping {

  const MIN_SIZE: usize = MIN_MAPPING_SIZE;

  fn expected_size(&self) -> PythResult<usize> {
    Ok(Self::MIN_SIZE + sizeof!(Pubkey) * self.product_count())
  }

  fn validate_payload(&self) -> PythResult<()> {
    check_field_gt!(self, num_, 0)?;
    check_field_le!(self, num_, c::PC_MAP_TABLE_SIZE)?;
    Ok(())
  }

}

impl_pyth_account!(Price, c::PC_ACCTYPE_PRICE);
impl_pyth_account!(Product, c::PC_ACCTYPE_PRODUCT);
impl_pyth_account!(Mapping, c::PC_ACCTYPE_MAPPING);

pub trait IsZero {

  fn is_zero(&self) -> bool;
  fn is_nonzero(&self) -> bool { ! self.is_zero() }

  fn if_nonzero(&self) -> Option<&Self> {
    match self.is_zero() {
      true => None,
      false => Some(self),
    }
  }

  fn err_if_zero(&self, name: impl Fn() -> String) -> PythResult<()> {
    match self.is_zero() {
      true => Err(PythError::NullPubkey { name: name() }.debug()),
      false => Ok(()),
    }
  }

}

impl IsZero for Pubkey {
  fn is_zero(&self) -> bool {
    // TODO Use oracle logic once bindgen inline functions work:
    // unsafe { c::pc_pub_key_is_zero(self as *const Pubkey) }
    Self::default().eq(self)
  }
}

#[test]
fn test_zero_key() {
  let zero = Pubkey::default();
  let nonzero = Pubkey::new_unique();
  assert!(zero.is_zero());
  assert!(zero.to_bytes().iter().all(|b| *b == 0 as u8));
  assert!(nonzero.is_nonzero());
}

/* Custom serialization of unadjusted prod_ array (see build.rs):

use serde::{Serializer, Deserializer};

const MAX_MAP_PRODUCTS: usize = c::PC_MAP_TABLE_SIZE as usize;
type MapProductKeys = [Pubkey; MAX_MAP_PRODUCTS];

impl Mapping {
  fn count_products(keys: &MapProductKeys) -> usize {
    for i in 0..keys.len() {
      if keys[i].is_zero() {
        return i;
      }
    }
    keys.len()
  }

  fn serialize_products<S: Serializer>(
    keys: &MapProductKeys,
    serializer: S,
  ) -> Result<S::Ok, S::Error> {
    let len = Self::count_products(keys);
    serialize_subarray(serializer, keys, len)
  }

  fn deserialize_products<'de, D: Deserializer<'de>>(
    deserializer: D,
  ) -> Result<MapProductKeys, D::Error> {
    const N: usize = MAX_MAP_PRODUCTS;
    let f = deserialize_subarray::<D, Pubkey, N>;
    let (keys, _len) = f(deserializer)?;
    Ok(keys)
  }
}
*/
