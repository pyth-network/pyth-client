use crate::{
  cast::as_static,
};

use std::{
  any::type_name,
  cell::{RefCell, RefMut},
  fmt,
  iter::Iterator,
  rc::Rc,
};

pub struct FnVec<'a, Inputs, Output> {
  // This should be FnMut<Inputs, Output=Output>
  // so multi-parameter callbacks don't need a wrapper tuple.
  pub fns: Vec<Rc<RefCell<dyn FnMut(Inputs) -> Output + 'a>>>,
}

impl<'a, I, O> FnVec<'a, I, O> {

  pub fn push(&mut self, f: impl FnMut(I) -> O + 'a) {
    self.fns.push(Rc::new(RefCell::new(f)));
  }

  pub fn iter(&self) -> impl Iterator<Item=RefMut<dyn FnMut(I) -> O + 'a>> {
    self.fns.iter().map(|f| f.borrow_mut())
  }

}

impl<'a, I, O> Default for FnVec<'a, I, O> {

  fn default() -> Self {
    Self{ fns: vec![] }
  }

}

impl<'a, I, O> Clone for FnVec<'a, I, O> {

  fn clone(&self) -> Self {
    Self{ fns: self.fns.clone() }
  }

}

impl<'a, I, O> fmt::Debug for FnVec<'a, I, O> {

  fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
    write!(f, "{}[{}]", type_name::<Self>(), self.fns.len())
  }

}

pub type Transform<'a, T> = FnVec<'a, T, T>;

impl<'a, T> Transform<'a, T> {

  pub fn call(&self, init: T) -> T {
    self.iter().fold(init, |o, mut f| f(o))
  }

}

pub type TryMap<'a, T, E> = FnVec<'a, T, Result<T, E>>;

impl<'a, T, E> TryMap<'a, T, E> {

  pub fn call(&self, mut val: T) -> Result<T, E> {
    for mut f in self.iter() {
      val = f(val)?;
    }
    Ok(val)
  }

}

// Input lifetime is for duration of call, not static.
pub type Fold<'a, I, O> = FnVec<'a, (&'static I, O), O>;

impl<'a, I: ?Sized, O> Fold<'a, I, O> {

  pub fn call(&self, input: &I, init: O) -> O {
    let input = unsafe{ as_static(input) };
    self.iter().fold(init, |o, mut f| f((input, o)))
  }

}

pub type Conditions<'a, Input> = FnVec<'a, &'static Input, bool>;

impl<'a, Input: ?Sized> Conditions<'a, Input> {

  pub fn all(&self, input: &Input) -> bool {
    let input = unsafe{ as_static(input) };
    self.iter().all(|mut f| f(input))
  }

  pub fn any(&self, input: &Input) -> bool {
    let input = unsafe{ as_static(input) };
    self.iter().any(|mut f| f(input))
  }

}
