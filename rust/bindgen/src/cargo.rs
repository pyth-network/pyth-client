use std::{
  env,
  path::{Path, PathBuf},
};

/// Tell cargo to rerun this build script if `path` changes.
pub fn rerun_if_path_changed(path: &Path) {
  println!("cargo:rerun-if-changed={}", path.to_str().unwrap());
}

/// Tell cargo to rerun this build script if this env var changes.
pub fn rerun_if_env_changed(key: &str) {
  println!("cargo:rerun-if-env-changed={}", key);
}

/// Add a compile-time variable accessible at run-time via env!().
pub fn set_rustc_env(key: &str, val: &str) {
  println!("cargo:rustc-env={}={}", key, val);
}

pub fn env_option(key: &str) -> Option<String> {
  rerun_if_env_changed(key);
  match env::var(key) {
    Ok(value) => Some(value),
    Err(_) => None
  }
}

pub fn env_or(key: &str, default: &str) -> String {
  env_option(key).unwrap_or(default.to_string())
}

pub fn env_or_bool(key: &str, default: bool) -> bool {
  env_or(key, &default.to_string()).parse::<bool>().unwrap()
}

pub fn env_or_path(key: &str, default: &str) -> PathBuf {
  let path = PathBuf::from(env_or(key, default));
  rerun_if_path_changed(&path);
  path
}

pub fn env_path_option(key: &str) -> Option<PathBuf> {
  match env_option(&key) {
    None => None,
    Some(path_str) => {
      let path = PathBuf::from(path_str);
      rerun_if_path_changed(&path);
      Some(path)
    }
  }
}
