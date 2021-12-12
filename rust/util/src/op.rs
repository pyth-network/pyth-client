use std::{
  cmp::Eq,
  cmp::Ord,
};

pub fn eq<T: Eq>(a: T, b: T) -> bool {
  a.eq(&b)
}

pub fn ne<T: Eq>(a: T, b: T) -> bool {
  a.ne(&b)
}

pub fn le<T: Ord>(a: T, b: T) -> bool {
  a.le(&b)
}

pub fn lt<T: Ord>(a: T, b: T) -> bool {
  a.lt(&b)
}

pub fn gt<T: Ord>(a: T, b: T) -> bool {
  a.gt(&b)
}

pub fn ge<T: Ord>(a: T, b: T) -> bool {
  a.ge(&b)
}
