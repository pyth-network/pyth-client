#![macro_use]

use crate::{
  cast::as_mut,
  debug::{
    DebugError,
    DebugResult,
  },
  valid::{
    Valid,
    IfValidRef,
    IfValidMut,
  },
};

use std::{
  io,
  slice,
};

use borsh::{
  BorshSerialize,
};

fn invalid_size(actual: usize, expected: usize) -> io::Error {
  let msg = format!("Expected {} bytes, got {}.", expected, actual);
  io::Error::new(io::ErrorKind::InvalidInput, msg).debug()
}

pub fn try_array<const N: usize>(buf: &[u8]) -> io::Result<&[u8; N]> {
  Ok(<&[u8; N]>::try_from(buf).or_else(|_| {
    Err(invalid_size(buf.len(), N))
  })?)
}

pub fn pop_array<'a, const N: usize>(
  buf: &mut &'a[u8],
) -> io::Result<
  &'a [u8; N]
> {
  match buf.get(..N) {
    None => {
      Err(invalid_size(buf.len(), N))
    },
    Some(popped) => {
      *buf = &buf[N..];
      unsafe {  // Length already checked.
        Ok(as_array!(N, popped))
      }
    },
  }
}

pub unsafe trait ByteCast : Sized + Valid {

  // TODO "const fn"
  fn min_size() -> usize;

  fn size(&self) -> usize {
    Self::min_size()
  }

  fn as_bytes(&self) -> &[u8] {
    unsafe {
      let ptr: *const u8 = (self as *const Self).cast();
      slice::from_raw_parts(ptr, self.size())
    }
  }

  fn from_bytes(bytes: &[u8]) -> IfValidRef<Self> {
    let len = bytes.len();
    let min = Self::min_size();
    if len < min {
      return Err(invalid_size(len, min).into());
    }
    let byte0 = &bytes[0];
    let res = unsafe{ as_type!(Self, byte0) }.if_valid()?;
    let size = res.size();
    if len < size {
      return Err(invalid_size(len, size).into());
    }
    Ok(res)
  }

  fn from_bytes_mut(bytes: &mut [u8]) -> IfValidMut<Self> {
    let res = Self::from_bytes(bytes)?;
    Ok(unsafe{ as_mut(res) })
  }

  fn from_borsh<'a>(buf: &mut &'a[u8]) -> io::Result<&'a Self> {
    let res = Self::from_bytes(buf).to_io()?;
    *buf = &buf[res.size()..];
    Ok(res)
  }

  fn write_borsh<W: io::Write>(&self, w: &mut W) -> io::Result<()> {
    BorshSerialize::serialize(self.as_bytes(), w)
  }

}

#[macro_export(local_inner_macros)]
macro_rules! impl_byte_cast {

  ($T: ty, $get_size: expr, $min_size: expr) => {

    unsafe impl ByteCast for $T {
      fn min_size() -> usize { $min_size }
      fn size(&self) -> usize { $get_size(self) }
    }

    // Static since borsh doesn't support generic lifetimes.
    // Prefer calling from_borsh directly for correctness.
    impl ::borsh::BorshDeserialize for &'static $T {
      fn deserialize(buf: &mut &[u8]) -> ::std::io::Result<Self> {
        let res = <$T as ByteCast>::from_borsh(buf)?;
        unsafe{
          Ok($crate::cast::as_static(res))
        }
      }
    }

    impl ::borsh::BorshDeserialize for $T {
      fn deserialize(buf: &mut &[u8]) -> ::std::io::Result<Self> {
        Ok(*<Self as ByteCast>::from_borsh(buf)?)
      }
    }

    impl ::borsh::BorshSerialize for $T {
      fn serialize<W: ::std::io::Write>(
        &self,
        writer: &mut W,
      ) -> ::std::io::Result<()> {
        <Self as ByteCast>::write_borsh(self, writer)
      }
    }

  };

  ($T: ty, $get_size: expr) => {
    impl_byte_cast!($T, $get_size, sizeof!($T));
  };

  ($T: ty) => {
    impl_byte_cast!($T, ::std::mem::size_of_val, sizeof!($T));
  };

}
