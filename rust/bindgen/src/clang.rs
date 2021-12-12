use std::path::Path;

fn join(flag: &str, value: &str) -> String {
  String::from(flag) + &value
}

pub fn std(std: &str) -> String {
  join("-std=", std)
}

pub fn include(dir: &Path) -> String {
  join("-I", dir.to_str().unwrap())
}

pub fn isystem(dir: &Path) -> String {
  join("-isystem", dir.to_str().unwrap())
}

pub fn sysroot(dir: &Path) -> String {
  join("--sysroot=", dir.to_str().unwrap())
}

pub fn define(def: &str) -> String {
  join("-D=", def)
}

pub fn warn(wflag: &str) -> String {
  join("-W", wflag)
}

pub fn flag(f: &str) -> String {
  join("-f", f)
}
