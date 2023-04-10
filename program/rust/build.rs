use {
    bindgen::Builder,
    std::{
        path::PathBuf,
        process::Command,
    },
};

fn main() {
    // Cargo exposes ENV variables for feature flags that can be used to determine whether to do a
    // self-contained build that compiles the C components automatically.
    if std::env::var("CARGO_FEATURE_LIBRARY").is_ok() {
        // OUT_DIR is the path cargo provides to a build directory under `target/` specifically for
        // isolated build artifacts. We use this to build the C program and then link against the
        // resulting static library. This allows building the program when used as a dependency of
        // another crate.
        let out_dir = std::env::var("OUT_DIR").unwrap();
        let out_dir = PathBuf::from(out_dir);

        // We must forward OUT_DIR as an env variable to the make script otherwise it will output
        // its artifacts to the wrong place.
        std::process::Command::new("make")
            .env("VERBOSE", "1")
            .env("OUT_DIR", out_dir.display().to_string())
            .current_dir("../c")
            .args(&["cpyth-native"])
            .status()
            .expect("Failed to build C program");

        // Emit instructions for cargo to link against the built static library.
        println!("cargo:rustc-link-lib=static=cpyth-native");
        println!("cargo:rustc-link-search={}", out_dir.display());
    } else {
        // Otherwise fall back to the old relative-path C style build we used to have.
        println!("cargo:rustc-link-search=./program/c/target");
    }

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
