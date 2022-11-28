use bindgen::Builder;

fn main() {
    println!("cargo:rustc-link-search=./program/c/target");

    // Generate and write bindings
    let bindings = Builder::default()
        .clang_arg("-I../../../solana/sdk/bpf/c/inc/")
        .header("./src/bindings.h")
        .rustfmt_bindings(true)
        .generate()
        .expect("Unable to generate bindings");
    bindings
        .write_to_file("./bindings.rs")
        .expect("Couldn't write bindings!");
}
