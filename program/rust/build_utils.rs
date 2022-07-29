use bindgen::callbacks::ParseCallbacks;
use std::collections::HashMap;
use std::panic::UnwindSafe;

///This type stores a hashmap from structnames
///to vectors of trait names, and ensures
///that the traits of each struct are added to its
///definition when an instance of this struct
///is provided as a ParseCallback for bindgen
#[derive(Debug, Default)]
pub struct DeriveAdderParserCallback<'a> {
    pub types_to_traits: HashMap<&'a str, Vec<String>>,
}

impl<'a> DeriveAdderParserCallback<'a> {
    ///create a parser that does not add any traits
    pub fn new() -> Self {
        Default::default()
    }
    //add pairs of types and their desired traits
    pub fn register_traits(&mut self, type_name: &'a str, traits: Vec<String>) {
        self.types_to_traits.insert(&type_name, traits);
    }
}

//this is required to implement the callback trait
impl UnwindSafe for DeriveAdderParserCallback<'_> {
}

impl ParseCallbacks for DeriveAdderParserCallback<'_> {
    fn add_derives(&self, _name: &str) -> Vec<String> {
        self.types_to_traits
            .get(_name)
            .unwrap_or(&Vec::<String>::new())
            .to_vec()
    }
}
