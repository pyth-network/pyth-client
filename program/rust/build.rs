use {
    bindgen::Builder,
    std::{
        path::PathBuf,
        process::Command,
    },
};

fn main() {
    println!("cargo:rustc-link-search=./program/c/target");

    // Generate and write bindings
    let bindings = Builder::default()
        .clang_arg(format!("-I{:}", get_solana_inc_path().display()))
        .header("./src/bindings.h")
        .rustfmt_bindings(true)
        .generate()
        .expect("Unable to generate bindings");
    bindings
        .write_to_file("./bindings.rs")
        .expect("Couldn't write bindings!");
}

/// Find the Solana C header bindgen
fn get_solana_inc_path() -> PathBuf {
    let which_stdout = Command::new("which")
        .args(["cargo-build-bpf"])
        .output()
        .unwrap()
        .stdout;
    let mut path = PathBuf::new();
    path.push(std::str::from_utf8(&which_stdout).unwrap());
    path.pop(); //
    let mut bpf_path = path.clone();
    // Older solana version have the SDK in the bpf/ folder, while newer version have
    // it in the sbf/ folder
    bpf_path.push("sdk/bpf/c/inc/");
    if bpf_path.exists() {
        bpf_path
    } else {
        path.push("sdk/sbf/c/inc/");
        path
    }
}
