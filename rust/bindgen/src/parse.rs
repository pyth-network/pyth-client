use crate::{
  util::{
    callback::{Transform, Fold},
    str::AsStr,
  },
};

use std::{
  collections::HashMap,
  panic::UnwindSafe,
};

use bindgen::{
  CargoCallbacks,
  callbacks::ParseCallbacks,
};

#[derive(Debug, Default, Clone)]
pub struct Parser<'a> {
  pub renames: Transform<'a, String>,
  pub derives: Fold<'a, str, Vec<String>>,
}

impl<'a> Parser<'a> {

  pub fn rename(&mut self, pairs: &[(&str, &str)]) {
    let mut map: HashMap<String, String> = Default::default();
    for (old, new) in pairs {
      let i = map.insert(old.to_string(), new.to_string());
      assert!(i.is_none(), "{} renamed multiple times", old);
    }
    self.renames.push(move |name| {
      map.get(&name).unwrap_or(&name).into()
    });
  }

  pub fn strip_prefix(&mut self, prefix: &str) {
    let prefix = prefix.to_owned();
    self.renames.push(move |name| {
      name.without_prefix(&prefix).into()
    });
  }

  pub fn strip_suffix(&mut self, suffix: &str) {
    let suffix = suffix.to_owned();
    self.renames.push(move |name| {
      name.without_suffix(&suffix).into()
    });
  }

  pub fn derive_all_from(&mut self, from: &str) {
    let from = from.to_owned();
    self.derives.push(move |(_name, mut output)| {
      output.push(from.to_string());
      output
    })
  }

  pub fn derive_types_from(&mut self, types: &[&str], from: &str) {
    let mut map: HashMap<String, String> = Default::default();
    for t in types {
      map.insert(t.to_string(), from.to_string());
    }
    self.derives.push(move |(name, mut output)| {
      if let Some(from) = map.get(name) {
        output.push(from.into());
      }
      output
    })
  }

  /// Clone as an input to [`bindgen::Builder::parse_callbacks`].
  pub fn boxed(&self) -> Box<dyn ParseCallbacks> {
    let ptr = self as *const Parser<'a>;
    let as_static: *const Parser<'static> = ptr.cast();
    Box::new(unsafe{ &*as_static }.clone())
  }

}

// Required by ParseCallbacks:
impl UnwindSafe for Parser<'_> {}

impl ParseCallbacks for Parser<'_> {

  fn item_name(&self, name: &str) -> Option<String> {
    Some(self.renames.call(name.into()))
  }

  fn include_file(&self, filename: &str) {
    // Rebuild whenever an included header file changes.
    CargoCallbacks{}.include_file(filename)
  }

  fn add_derives(&self, name: &str) -> Vec<String> {
    self.derives.call(name, vec![])
  }

}
