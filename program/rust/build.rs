mod build_utils;
use bindgen::Builder;
use std::vec::Vec;


fn main() {
    println!("cargo:rustc-link-search=../c/target");
    
    let borsh_derives: Vec<String> = vec!["BorshSerialize".to_string(), "BorshDeserialize".to_string()];

    //make a parser and to it type, traits pairs
    let mut parser = build_utils::DeriveAdderParserCallback::new();
    parser.register_traits("cmd_hdr", borsh_derives);

    //generate and write bindings
    let bindings = Builder::default().header("./src/bindings.h").parse_callbacks(Box::new(parser)).rustfmt_bindings(true).generate().expect("Unable to generate bindings");
    bindings.write_to_file("./bindings.rs").expect("Couldn't write bindings!");
}
