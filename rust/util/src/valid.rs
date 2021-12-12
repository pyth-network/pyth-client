use crate::{
  debug::DebugError,
};

use std::io;

pub type IfValidRef<'a, T> = Result<&'a T, <T as Valid>::Error>;
pub type IfValidMut<'a, T> = Result<&'a mut T, <T as Valid>::Error>;

pub trait Valid {

  type Error: DebugError + From<io::Error>;

  fn validate(&self) -> Result<(), Self::Error>;

  fn if_valid(&self) -> IfValidRef<Self> {
    self.validate()?;
    Ok(self)
  }

  fn if_valid_mut(&mut self) -> IfValidMut<Self> {
    self.validate()?;
    Ok(self)
  }

}
