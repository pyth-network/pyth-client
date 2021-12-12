use std::{
  vec::Vec,  // TryInto<[T; N]>
};

use serde::de::{
  Deserialize,
  Deserializer,
  Error as DeserError,
};

use serde::ser::{
  Serialize,
  Serializer,
  Error as SerError,
};

fn invalid_len(sub_len: usize, max_len: usize) -> String {
  let msg = format!(
    "Invalid subarray length {} for full array of length {}.",
    sub_len,
    max_len,
  );
  debug_assert!(false, "{}", msg);
  msg
}

pub fn serialize_subarray<
  S: Serializer,
  T: Serialize,
  const N: usize,
>(
  serializer: S,
  arr: &[T; N],
  len: usize,
) -> Result<S::Ok, S::Error> {
  match arr.get(..len) {
    None => Err(S::Error::custom(invalid_len(len, arr.len()))),
    Some(subarr) => subarr.serialize(serializer),
  }
}

pub fn deserialize_subarray<
  'de,
  D: Deserializer<'de>,
  T: Deserialize<'de> + Default,
  const N: usize,
>(deserializer: D) -> Result<([T; N], usize), D::Error> {

  let mut vec = Vec::<T>::deserialize(deserializer)?;
  let len = vec.len();
  if len < N {
    vec.resize_with(N, T::default);
  }

  match <[T; N]>::try_from(vec) {
    Err(_) => Err(D::Error::custom(invalid_len(len, N))),
    Ok(arr) => Ok((arr, len)),
  }

}
