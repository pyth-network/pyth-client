use {
    bindgen::Builder,
    std::{
        path::PathBuf,
        process::Command,
    },
};

fn main() {
    let target_arch = std::env::var("CARGO_CFG_TARGET_ARCH").unwrap();

    // The tests depend on the BPF build (because we load the BPF program into the solana simulator).
    // Always build the bpf binary before doing anything else.
    if target_arch != "bpf" {
        // cargo-build-bpf does not get information about features
        // from this script. We need to fish them out and pass them
        // manually.
        let mut features = vec![];
        for (env, _val) in std::env::vars() {
            if env.starts_with("CARGO_FEATURE_") {
                features.push(env.trim().replace("CARGO_FEATURE_", "").to_lowercase());
            }
        }

        let mut cmd = Command::new("cargo");
        cmd.arg("build-bpf");

        if !features.is_empty() {
            cmd.arg("--features");
            cmd.args(features);
        }

        let status = cmd.status().unwrap();

        if !status.success() {
            panic!(
                "cargo-build-bpf did not exit with 0 (code {:?})",
                status.code()
            );
        }
    }

    // OUT_DIR is the path cargo provides to a build directory under `target/` specifically for
    // isolated build artifacts. We use this to build the C program and then link against the
    // resulting static library. This allows building the program when used as a dependency of
    // another crate.
    let out_dir = std::env::var("OUT_DIR").unwrap();
    let out_dir = PathBuf::from(out_dir);

    let mut make_targets = vec![];
    if target_arch == "bpf" {
        make_targets.push("cpyth-bpf");
    } else {
        make_targets.push("cpyth-native");
    }
    make_targets.push("test");

    // We must forward OUT_DIR as an env variable to the make script otherwise it will output
    // its artifacts to the wrong place.
    std::process::Command::new("make")
        .env("VERBOSE", "1")
        .env("OUT_DIR", out_dir.display().to_string())
        .current_dir("../c")
        .args(make_targets)
        .status()
        .expect("Failed to build C program");

    // Link against the right library for the architecture
    if target_arch == "bpf" {
        println!("cargo:rustc-link-lib=static=cpyth-bpf");
    } else {
        println!("cargo:rustc-link-lib=static=cpyth-native");
    }
    println!("cargo:rustc-link-lib=static=cpyth-test");
    println!("cargo:rustc-link-search={}", out_dir.display());

    std::fs::create_dir("./codegen").unwrap_or_else(|e| {
        eprintln!(
            "Could not create codegen directory (may exist which is fine), error: {}",
            e
        );
    });

    // Generate and write bindings
    let bindings = Builder::default()
        .clang_arg(format!("-I{:}", get_solana_inc_path().display()))
        .header("./src/bindings.h")
        .rustfmt_bindings(true)
        .generate()
        .expect("Unable to generate bindings");
    bindings
        .write_to_file("./codegen/bindings.rs")
        .expect("Couldn't write bindings!");

    // Rerun the build script if either the rust or C code changes
    println!("cargo:rerun-if-changed=../")
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
