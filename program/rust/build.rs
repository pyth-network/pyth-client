mod build_utils;
use bindgen::Builder;

fn main() {
    println!("cargo:rustc-link-search=./program/c/target");

    let borsh_derives = ["BorshSerialize".to_string(), "BorshDeserialize".to_string()];
    let pod_derives = ["Pod".to_string(), "Zeroable".to_string()];

    //make a parser and to it type, traits pairs
    let mut parser = build_utils::DeriveAdderParserCallback::new();
    parser.register_traits("cmd_hdr", borsh_derives.to_vec());
    parser.register_traits("pc_acc", borsh_derives.to_vec());
    parser.register_traits("pc_price_info", borsh_derives.to_vec());
    parser.register_traits("cmd_upd_price", borsh_derives.to_vec());
    parser.register_traits("pc_ema", borsh_derives.to_vec());
    parser.register_traits("pc_price", pod_derives.to_vec());
    parser.register_traits("pc_prod", pod_derives.to_vec());
    parser.register_traits("pc_map_table", pod_derives.to_vec());


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
