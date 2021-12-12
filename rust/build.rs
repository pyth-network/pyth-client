use pyth_bindgen::{
  Generator,
  Builder,
  util::str::{AsStr, AsStrMut},
};

use std::{
  cell::RefCell,
  collections::HashSet,
};

fn main() {
  let pyth_types: RefCell<HashSet<String>> = Default::default();
  let blocklist: RefCell<HashSet<String>> = Default::default();

  let mut gen = Generator::default();
  gen.derive_debug();
  gen.derive_copy();

  gen.setup.push(|mut builder: Builder| {
    for t in blocklist.borrow().iter() {
      builder = builder.blocklist_type(t);
    }
    // Disable Default since it doesn't support arrays larger than 32.
    builder = builder.derive_default(false);
    builder
  });

  gen.import("solana_sdk::pubkey::Pubkey");
  blocklist.borrow_mut().extend([
    // Replaced with rust equivalent:
    "union_pc_pub_key",
    "pc_pub_key_t",
    // Unused:
    "struct_pc_str",
    "pc_str_t",
    "struct_sysvar_clock",
    "sysvar_clock_t",
  ].map(str::to_string));

  gen.parser.renames.push(|mut name: String| {

    if name.starts_with_any([
      "Sol",
      "struct_Sol",
      "enum_Sol",
      "union_Sol",
    ]) {
      blocklist.borrow_mut().insert(name.clone());
    }

    else if name.eq("pc_pub_key_t") {
      name.set("Pubkey");
    }

    /*
    else if name.ends_with("_t") {
      for prefix in ["pc_", "cmd_"] {
        if name.starts_with(prefix) {
          name.del_prefix(prefix);
          name.del_suffix("_t");
          name.make_pascal();
        }
      }
    }
     */

    else {
      for prefix in [
        "struct_pc",
        "union_pc",
        "enum_pc",
      ] {
        if name.starts_with(prefix) {
          // name.del_prefix(prefix);
          // name.make_pascal();
          pyth_types.borrow_mut().insert(name.clone());
        }
      }
    }

    name
  });

  gen.import("serde::{Serialize, Deserialize}");
  gen.parser.derives.push(|(name, mut output) | {
    if pyth_types.borrow().contains(name) {
      output.extend([
        "Serialize",
        "Deserialize",
      ].map(str::to_string));
    }
    output
  });

  // Currently alignment assertions fail (expected 8, get 4).
  // Ideally bindgen would add #[repr(align(...))].
  gen.test_layout = false;

  // Disable serialization of Mapping::prod_, since true len is dynamic.
  // Products are serialized via client::MappingAccount.
  gen.sed(concat!(
    r#"s/^\( *\)\(pub prod_: \[Pubkey; .*],\)$/"#,
    r#"\1#[serde(skip_serializing, skip_deserializing, default = "default_mapping_products")]\n"#,
    r#"\1\2/g"#,
  ));

  // Needed for "#[serde(skip_deserializing)]"
  gen.output_suffix.push([
    "",
    "fn default_mapping_products() -> [Pubkey; PC_MAP_TABLE_SIZE as usize] {",
    "    [Pubkey::default(); PC_MAP_TABLE_SIZE as usize]",
    "}",
  ].join("\n"));

  gen.run().unwrap();
}
