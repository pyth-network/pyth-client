use std::{
  error::Error as StdError,
  fmt::Debug,
  io,
};

pub trait DebugError: Sized + Debug {

  fn debug(self) -> Self {
    debug_assert!(false, "{:?}", self);
    self
  }

  fn to_io(self) -> io::Error;

}

impl<E: Sized + Debug> DebugError for E
where E: Into<Box<dyn StdError + Send + Sync>> {

  fn to_io(self) -> io::Error {
    io::Error::new(
      io::ErrorKind::Other,
      self.into(),
    )
  }

}

pub trait DebugResult<T> {
  fn debug(self) -> Self;
  fn to_io(self) -> io::Result<T>;
}

impl<T, E: DebugError> DebugResult<T> for Result<T, E> {

  fn debug(self) -> Self {
    debug_assert!(self.is_ok(), "{:?}", self.err().unwrap());
    self
  }

  fn to_io(self) -> io::Result<T> {
    Ok(self.or_else(|err| Err(err.to_io()))?)
  }

}
