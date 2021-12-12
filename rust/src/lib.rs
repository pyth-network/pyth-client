/// C Bindings
pub mod c {
  #![allow(non_upper_case_globals)]
  #![allow(non_camel_case_types)]
  #![allow(non_snake_case)]
  include!(concat!(env!("OUT_DIR"), "/oracle.rs"));
}

/// `ver_` field of all pyth accounts and commands.
pub const VERSION: u32 = c::PC_VERSION;

/// Published component price for a contributing provider.
pub type PriceComponent = c::pc_price_comp_t;

/// Price, confidence, etc., for component and aggregate prices.
pub type PriceInfo = c::pc_price_info_t;

/// Time-weighted exponential moving average.
pub type Ema = c::pc_ema_t;

/// Attr name or value for an [`acc::Product`].
pub type AttrStr = c::pc_str_t;

/// Account data for [`solana_program::sysvar::clock`].
pub type SysvarClock = c::sysvar_clock_t;

/// Values for [`acc::Price`]`::type_`.
#[repr(u32)]
#[derive(Debug)]
pub enum PriceType {
  Unknown = c::PC_PTYPE_UNKNOWN,
  Price = c::PC_PTYPE_PRICE,
}

/// Values for [`PriceInfo`]`::status_`.
#[repr(u32)]
#[derive(Debug)]
pub enum Status {
  Unknown = c::PC_STATUS_UNKNOWN,
  Trading = c::PC_STATUS_TRADING,
  Halted = c::PC_STATUS_HALTED,
  Aucton = c::PC_STATUS_AUCTION,
}

/// Account Types
pub mod acc {
  use super::c;

  /// Hash table of symbol to price account mappings.
  pub type Mapping = c::pc_map_table_t;

  /// Product reference data.
  pub type Product = c::pc_prod_t;

  /// Price account containing aggregate and all component prices.
  pub type Price = c::pc_price_t;

  /// PC representation of [`solana_program::pubkey::Pubkey`].
  pub type Key = c::pc_pub_key_t;

  /// `magic_` field of all pyth accounts.
  pub const MAGIC: u32 = c::PC_MAGIC;

  impl Mapping {
    pub const TYPE: u32 = c::PC_ACCTYPE_MAPPING;

    /// Max number of product keys in `prod_` field.
    pub const MAX_PRODUCTS: u32 = c::PC_MAP_TABLE_SIZE;
  }

  impl Product {
    pub const TYPE: u32 = c::PC_ACCTYPE_PRODUCT;
  }

  impl Price {
    pub const TYPE: u32 = c::PC_ACCTYPE_PRICE;

    /// Max number of [`PriceComponent`]s in `comp_` field.
    pub const MAX_COMPONENTS: u32 = c::PC_COMP_SIZE;
  }
}

/// Instructions
pub mod cmd {
  use super::c;

  /// Initialize and add new product reference data account.
  type AddProduct = c::cmd_add_product_t;

  /// Update product account.
  type UpdateProduct = c::cmd_upd_product_t;

  /// Add new price account to a product account.
  type AddPrice = c::cmd_add_price_t;

  /// Add publisher to symbol account.
  type AddPublisher = c::cmd_add_publisher_t;

  /// Delete publisher from symbol account.
  type DelPublisher = c::cmd_del_publisher_t;

  /// Publish component price.
  type UpdatePrice = c::cmd_upd_price_t;

  /// (Re)initialize price account.
  type InitPrice = c::cmd_init_price_t;

  /// Run aggregate price test.
  type UpdateTest = c::cmd_upd_test_t;

  /// Set min publishers
  type SetMinPub = c::cmd_set_min_pub_t;

  impl AddProduct {
    pub const TYPE: i32 = c::command_t_e_cmd_add_product as i32;
  }

  impl UpdateProduct {
    pub const TYPE: i32 = c::command_t_e_cmd_upd_product as i32;
  }

  impl AddPrice {
    pub const TYPE: i32 = c::command_t_e_cmd_add_price as i32;
  }

  impl AddPublisher {
    pub const TYPE: i32 = c::command_t_e_cmd_add_publisher as i32;
  }

  impl DelPublisher {
    pub const TYPE: i32 = c::command_t_e_cmd_del_publisher as i32;
  }

  impl UpdatePrice {
    pub const TYPE: i32 = c::command_t_e_cmd_upd_price as i32;
  }

  impl InitPrice {
    pub const TYPE: i32 = c::command_t_e_cmd_init_price as i32;
  }

  impl UpdateTest {
    pub const TYPE: i32 = c::command_t_e_cmd_upd_test as i32;
  }

  impl SetMinPub {
    pub const TYPE: i32 = c::command_t_e_cmd_set_min_pub as i32;
  }
}
