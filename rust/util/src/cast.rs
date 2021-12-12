#![macro_use]

use std::{
  cell::UnsafeCell,
};

pub unsafe fn as_type<IntoT, FromT>(x: &FromT) -> &IntoT {
  &*(x as *const FromT as *const IntoT)
}

pub unsafe fn as_type_mut<IntoT, FromT>(x: &mut FromT) -> &mut IntoT {
  &mut *(x as *mut FromT as *mut IntoT)
}

pub unsafe fn as_mut<T: ?Sized>(x: &T) -> &mut T {
  let cell = &*(x as *const T as *const UnsafeCell<T>);
  &mut *cell.get()
}

pub unsafe fn as_array<T, const N: usize>(r: &[T]) -> &[T; N] {
  &*(r.as_ptr() as *const [T; N])
}

pub unsafe fn as_array_mut<T, const N: usize>(r: &mut [T]) -> &mut [T; N] {
  &mut *(r.as_mut_ptr() as *mut [T; N])
}

pub unsafe fn as_static<T: ?Sized>(x: &T) -> &'static T {
  &*(x as *const T)
}

pub unsafe fn as_static_mut<T: ?Sized>(x: &mut T) -> &'static mut T {
  &mut *(x as *mut T)
}

#[macro_export]
macro_rules! as_type {
  ($T: tt, $ref: expr) => {
    $crate::cast::as_type::<$T, _>($ref)
  }
}

#[macro_export]
macro_rules! as_type_mut {
  ($T: tt, $ref: expr) => {
    $crate::cast::as_type_mut::<$T, _>($ref)
  }
}

#[macro_export]
macro_rules! reinterpret {
 ($T: tt, $value: expr) => {
    ::std::mem::transmute::<_, $T>($value)
  }
}

#[macro_export]
macro_rules! as_array {
 ($N: expr, $ref: expr) => {
    $crate::cast::as_array::<_, $N>($ref)
  }
}

#[macro_export]
macro_rules! sizeof {
  ($T: tt) => {
    ::std::mem::size_of::<$T>()
  }
}

#[macro_export(local_inner_macros)]
macro_rules! assert_type_size {
  ($T: tt, $size: expr) => {
    $crate::sa::const_assert_eq!(sizeof!($T), $size);
  }
}

#[macro_export(local_inner_macros)]
macro_rules! assert_same_size {
  ($T1: tt, $T2: tt) => {
    assert_type_size!($T1, sizeof!($T2));
  }
}

#[macro_export(local_inner_macros)]
macro_rules! impl_cast_from_to {
  ($FromT: path, $IntoT: path) => {

    assert_same_size!($FromT, $IntoT);

    impl From<&$FromT> for &$IntoT {
      fn from(from_ref: &$FromT) -> Self {
        unsafe {
          as_type!(Self, from_ref)
        }
      }
    }

    impl From<$FromT> for $IntoT {
      fn from(from_val: $FromT) -> Self {
        unsafe {
          reinterpret!(Self, from_val)
        }
      }
    }

  };
}

#[macro_export(local_inner_macros)]
macro_rules! impl_cast_between {
  ($T1: path, $T2: path) => {
    impl_cast_from_to!($T1, $T2);
    impl_cast_from_to!($T2, $T1);
  };
}

#[cfg(test)]
mod tests {

  macro_rules! test_cast {
    ($FromT: tt, $IntoT: tt, $from_val: expr) => {
      unsafe {
        let into_val: $IntoT = reinterpret!($IntoT, $from_val);
        assert_eq!($from_val, reinterpret!($FromT, into_val));
        for into_ref in [&into_val, as_type!($IntoT, &$from_val)] {
          assert_eq!($from_val, *as_type!($FromT, into_ref));
        }
      }
    }
  }

  #[test]
  fn test_utup_to_itup() {
    test_cast!(
      (u8, u32, u64),
      (i8, i32, i64),
      (u8::MAX, u32::MAX, u64::MAX)
    );
  }
}
