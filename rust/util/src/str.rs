pub trait AsStr: AsRef<str> {

  fn str(&self) -> &str {
    self.as_ref()
  }

  fn or_self(&self, f: impl Fn(&str) -> Option<&str>) -> &str {
    let old = self.str();
    match f(old) {
      None => old,
      Some(new) => new,
    }
  }

  fn without_prefix(&self, prefix: &str) -> &str {
    self.or_self(|s| s.strip_prefix(prefix))
  }

  fn without_suffix(&self, suffix: &str) -> &str {
    self.or_self(|s| s.strip_suffix(suffix))
  }

  fn strip_prefixes<
    'a, I: IntoIterator<Item = &'a str>
  >(&self, prefixes: I) -> &str {
    prefixes.into_iter().fold(self.str(), |s: &str, prefix: &'a str| {
      s.without_prefix(prefix)
    })
  }

  fn strip_suffixes<
    'a, I: IntoIterator<Item = &'a str>
  >(&self, suffixes: I) -> &str {
    suffixes.into_iter().fold(self.str(), |s: &str, suffix: &'a str| {
      s.without_suffix(suffix)
    })
  }

  fn starts_with_any<
    'a, I: IntoIterator<Item = &'a str>
  >(&self, prefixes: I) -> bool {
    prefixes.into_iter().any(|prefix: &'a str| {
      self.str().starts_with(prefix)
    })
  }

  fn ends_with_any<
    'a, I: IntoIterator<Item = &'a str>
  >(&self, suffixes: I) -> bool {
    suffixes.into_iter().any(|suffix: &'a str| {
      self.str().ends_with(suffix)
    })
  }

  fn is_in<
    'a, I: IntoIterator<Item = &'a str>
  >(&self, strs: I) -> bool {
    strs.into_iter().any(|s: &'a str| {
      self.str().eq(s)
    })
  }

  fn is_lower(&self) -> bool {
    return self.str().eq(&self.str().to_lowercase())
  }

  fn is_upper(&self) -> bool {
    return self.str().eq(&self.str().to_uppercase())
  }

  fn to_pascal(&self) -> String {
    let old = self.str();
    let mut new = String::with_capacity(old.len());
    for tok in old.split('_') {
      if tok.len() > 0 {
        let i = new.len();
        new.push_str(tok);
        let first: &mut str = unsafe { new.get_unchecked_mut(i..i+1) };
        first.make_ascii_uppercase();
      }
    }
    new
  }
}

pub trait AsStrMut: AsStr + From<String> {

  fn set<S: AsRef<str>>(&mut self, s: S) {
    if ! self.str().eq(s.as_ref()) {
      *self = Self::from(String::from(s.as_ref()))
    }
  }

  fn del_prefix(&mut self, prefix: &str) {
    self.set(self.without_prefix(prefix).to_owned())
  }

  fn del_suffix(&mut self, suffix: &str) {
    self.set(self.without_suffix(suffix).to_owned())
  }

  fn make_pascal(&mut self) {
    self.set(self.to_pascal())
  }

}

impl AsStr for str {}
impl AsStr for String {}
impl AsStrMut for String {}
