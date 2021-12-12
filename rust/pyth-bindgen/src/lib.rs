use std::env;
use std::path::{Path, PathBuf};

use bindgen;

pub fn rerun_if_path_changed(path: &Path) {
  println!("cargo:rerun-if-changed={}", path.to_str().unwrap());
}

pub fn rerun_if_env_changed(key: &str) {
  println!("cargo:rerun-if-env-changed={}", key);
}

pub fn env_or(key: &str, default: &str) -> String {
  rerun_if_env_changed(key);
  env::var(key).unwrap_or(default.to_string())
}

pub fn env_out_dir() -> PathBuf {
  PathBuf::from(env::var("OUT_DIR").unwrap())
}

pub mod args {
  use std::path::Path;

  pub fn arg(flag: &str, value: &str) -> String {
    String::from(flag) + &value
  }

  pub fn std(std: &str) -> String {
    arg("-std=", std)
  }

  pub fn include(include_dir: &Path) -> String {
    arg("-I", include_dir.to_str().unwrap())
  }

  pub fn isystem(include_dir: &Path) -> String {
    arg("-isystem", include_dir.to_str().unwrap())
  }

  pub fn define(def: &str) -> String {
    arg("-D=", def)
  }

  pub fn warn(def: &str) -> String {
    arg("-W", def)
  }

  pub fn flag(def: &str) -> String {
    arg("-f", def)
  }
}

pub struct Generator {
  pub std: String,
  pub define_bpf: bool,
  pub define_test: bool,
  pub no_builtin: bool,
  pub solana: PathBuf,
  pub src_dir: PathBuf,
  pub out_dir: PathBuf,
  pub extra_args: Vec<String>,
}

impl Default for Generator {
  fn default() -> Self {
    Self{
      std: String::from("c17"),
      define_bpf: true,
      define_test: false,
      no_builtin: true,
      solana: PathBuf::from("../../solana"),
      src_dir: PathBuf::from("../program/src"),
      extra_args: vec![],
      out_dir: env_out_dir(),
    }
  }
}

impl Generator {

  pub fn bind_header(&self, header: &Path) -> std::io::Result<()> {
    let out_path = &self.out_rs_path(header);
    rerun_if_path_changed(header);
    rerun_if_path_changed(&out_path);
    let builder = self.builder(header);
    let bindings = builder.generate().expect("Failed to generate bindings.");
    bindings.write_to_file(&out_path)
  }

  pub fn out_rs_path(&self, header: &Path) -> PathBuf {
    let stem = header.file_stem().unwrap();
    self.out_dir.join(stem).with_extension("rs")
  }

  pub fn solana_inc_dir(&self) -> PathBuf {
    self.solana.join("sdk/bpf/c/inc")
  }

  pub fn solana_header(&self) -> PathBuf {
    self.solana_inc_dir().join("solana_sdk.h")
  }

  pub fn clang_args(&self) -> Vec<String> {
    let mut args = self.extra_args.clone();

    args.extend([
      args::std(&self.std),
      args::include(&self.src_dir),
      args::isystem(&self.solana_inc_dir()),
    ]);

    for warn in ["all", "extra", "conversion", "error"] {
      args.push(args::warn(warn));
    }
    if self.no_builtin {
      args.push(args::flag("no-builtin"));
    }
    if self.define_bpf {
      args.push(args::define("__bpf__"));
    }
    if self.define_test {
      args.push(args::define("SOL_TEST"));
    }

    args
  }

  pub fn builder(&self, header: &Path) -> bindgen::Builder {
    let args = self.clang_args();
    println!("clang args: {}", args.join(" "));
    let mut builder = bindgen::Builder::default();
    builder = builder.header(header.to_str().unwrap());
    builder = builder.clang_args(args.iter());
    // Rebuild whenever an included header file changes.
    builder.parse_callbacks(Box::new(bindgen::CargoCallbacks))
  }
}
