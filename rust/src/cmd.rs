use crate::{
  bindings as c,
  error::{
    PythError,
    PythResult,
    check_field_eq,
  },
  util::{
    bytes::ByteCast,
    valid::Valid,
  },
};

pub use c::{
  cmd_hdr_t as CmdHeader,
  cmd_add_price_t as AddPrice,
  cmd_add_product_t as AddProduct,
  cmd_add_publisher_t as AddPublisher,
  cmd_del_publisher_t as DelPublisher,
  cmd_init_price_t as InitPrice,
  cmd_set_min_pub_t as SetMinPub,
  cmd_upd_price_t as UpdatePrice,
  cmd_upd_product_t as UpdateProduct,
  cmd_upd_test_t as UpdateTest,
};

pub trait Command: Sized {

  const TYPE: u32;

  fn header(&self) -> &CmdHeader {
    unsafe {
      as_type!(CmdHeader, self)
    }
  }

  fn validate(&self) -> PythResult<()> {
    let h = self.header();
    check_field_eq!(h, ver_, c::PC_VERSION)?;
    check_field_eq!(h, cmd_, Self::TYPE as i32)
  }

}

macro_rules! impl_command {
  ($T: ty, $cmd: expr) => {

    impl Command for $T {
      const TYPE: u32 = $cmd;
    }

    impl Valid for $T {
      type Error = PythError;
      fn validate(&self) -> PythResult<()> {
        <Self as Command>::validate(self)
      }
    }

    impl_byte_cast!($T);
  };
}

impl_command!(AddPrice, c::e_cmd_add_price);
impl_command!(AddProduct, c::e_cmd_add_product);
impl_command!(AddPublisher, c::e_cmd_add_publisher);
impl_command!(DelPublisher, c::e_cmd_del_publisher);
impl_command!(InitPrice, c::e_cmd_init_price);
impl_command!(SetMinPub, c::e_cmd_set_min_pub);
impl_command!(UpdatePrice, c::e_cmd_upd_price);
impl_command!(UpdateProduct, c::e_cmd_upd_product);
impl_command!(UpdateTest, c::e_cmd_upd_test);
