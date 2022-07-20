use bindgen;
use std::panic::UnwindSafe;
use std::collections::HashMap;
///This type stores a hashmap from structnames
///to vectors of trait names, and ensures
///that the traits of each struct are added to its
///definition when an instance of this struct
///is provided as a ParseCallback for bindgen
#[derive(Debug)]
pub struct DeriveAdderParserCallback<'a>{
    pub types_to_traits: HashMap<&'a str, Vec<String>> ,
}

//this is required to implement the callback trait
impl UnwindSafe for DeriveAdderParserCallback<'_> {}

impl bindgen::callbacks::ParseCallbacks for DeriveAdderParserCallback<'_>{
    fn add_derives(&self, _name: &str) -> Vec<String>{
        let traits = self.types_to_traits.get(_name);
        match traits{
            Some(trait_names)=>trait_names.to_vec(),
            None => vec![],
        }
    }
}