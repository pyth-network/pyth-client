mod build_utils;
use bindgen::Builder;
use std::collections::HashMap;
use std::vec::Vec;


fn main() {
    println!("cargo:rustc-link-search=../c/target");
    
    let borsh_derives: Vec<String> = vec!["BorshSerialize".to_string(), "BorshDeserialize".to_string()];

    let parser = build_utils::DeriveAdderParserCallback{types_to_traits: HashMap::from([
        ("cmd_hdr", borsh_derives)
        //map more struct names to traits here, be sure that the definitions of your traites
        //are included in c_oracle_header.rs
    ])};

    //generate and write bindings
    let bindings = Builder::default().header("./src/bindings.h").parse_callbacks(Box::new(parser)).rustfmt_bindings(true).generate().expect("Unable to generate bindings");
    bindings.write_to_file("./bindings.rs").expect("Couldn't write bindings!");
}
