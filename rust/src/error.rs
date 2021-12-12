use crate::{
  util::{
    debug::DebugError,
  }
};

use std::{
  error::Error as StdError,
  fmt,
  io,
  marker::{Send, Sync},
  str,
};

pub type FieldVal = i64;
pub type BoxError = Box<dyn StdError + Send + Sync>;
pub type BoxResult<T> = Result<T, BoxError>;

#[derive(Debug)]
pub enum PythError {
  Boxed { err: BoxError },
  InvalidField { name: String, actual: FieldVal, expected: FieldVal},
  NullPubkey { name: String },
  DuplicateAttr { attr: String },
  MissingAttr { attr: String },
  UnpairedAttr,
}

impl StdError for PythError {}

impl fmt::Display for PythError {
  fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
    fmt::Debug::fmt(self, f)
  }
}

impl PythError {
  pub fn debug(self) -> Self {
    DebugError::debug(self)
  }
}

pub type PythResult<T> = Result<T, PythError>;

pub fn to_pyth_err<E: Into<BoxError>>(err: E) -> PythError {
  PythError::Boxed { err: err.into() }.debug()
}

macro_rules! impl_to_pyth_err {
  ($E: ty) => {
    impl From<$E> for PythError {
      fn from(err: $E) -> Self {
        $crate::error::to_pyth_err(err)
      }
    }
  }
}

impl_to_pyth_err!(io::Error);
impl_to_pyth_err!(str::Utf8Error);

pub fn validate_field<T: Copy + Into<FieldVal>>(
  name: &str,
  actual: T,
  expected: T,
  valid: impl Fn(T, T) -> bool,
) -> PythResult<()> {
  match valid(actual, expected) {
    true => Ok(()),
    false => Err(PythError::InvalidField{
      name: name.into(),
      actual: actual.into(),
      expected: expected.into(),
    }.debug())
  }
}

macro_rules! check_field {
  ($obj: expr, $field: ident, $expected: expr, $op: expr) => {
    $crate::error::validate_field(
      stringify!($field),
      $obj.$field,
      $expected,
      $op,
    )
  }
}

macro_rules! check_field_eq {
  ($o: expr, $f: ident, $e: expr) => {
    $crate::error::check_field!($o, $f, $e, $crate::util::op::eq)
  }
}

macro_rules! check_field_ge {
  ($o: expr, $f: ident, $e: expr) => {
    $crate::error::check_field!($o, $f, $e, $crate::util::op::ge)
  }
}

macro_rules! check_field_gt {
  ($o: expr, $f: ident, $e: expr) => {
    $crate::error::check_field!($o, $f, $e, $crate::util::op::gt)
  }
}

macro_rules! check_field_le {
  ($o: expr, $f: ident, $e: expr) => {
    $crate::error::check_field!($o, $f, $e, $crate::util::op::le)
  }
}

macro_rules! check_field_in {
  ($obj: expr, $field: ident, $container: expr) => {{
    $crate::error::check_field!(
      $obj,
      $field,
      *$container.iter().next().unwrap(),
      |actual, _expected| $container.contains(&actual)
    )
  }}
}

pub(crate) use {
  check_field,
  check_field_eq,
  check_field_ge,
  check_field_gt,
  check_field_le,
  check_field_in,
};
