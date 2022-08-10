mod build_utils;
use bindgen::Builder;

fn main() {
    println!("cargo:rustc-link-search=./program/c/target");

    //make a parser and to it type, traits pairs
    let parser = build_utils::DeriveAdderParserCallback::new();

    //generate and write bindings
    let bindings = Builder::default()
        .header("./src/bindings.h")
        .parse_callbacks(Box::new(parser))
        .rustfmt_bindings(true)
        .generate()
        .expect("Unable to generate bindings");
    bindings
        .write_to_file("./bindings.rs")
        .expect("Couldn't write bindings!");
}
