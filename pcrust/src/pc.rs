pub const MAP_TABLE_SIZE : usize = 640;
pub const PROD_ACCT_SIZE : usize = 512;
pub const PROD_HDR_SIZE  : usize = 48;
pub const PROD_ATTR_SIZE : usize = PROD_ACCT_SIZE - PROD_HDR_SIZE;

#[repr(C)]
pub struct AccKey
{
  pub val: [u8;32]
}

#[repr(C)]
pub struct Mapping
{
  pub magic      : u32,        // pyth magic number
  pub ver        : u32,        // program version
  pub atype      : u32,        // account type
  pub size       : u32,        // account used size
  pub num        : u32,        // number of product accounts
  pub unused     : u32,
  pub next       : AccKey,     // next mapping account (if any)
  pub products   : [AccKey;MAP_TABLE_SIZE]
}

#[repr(C)]
pub struct Product
{
  pub magic      : u32,        // pyth magic number
  pub ver        : u32,        // program version
  pub atype      : u32,        // account type
  pub size       : u32,        // price account size
  pub px_acc     : AccKey,     // first price account in list
  pub attr       : [u8;PROD_ATTR_SIZE]
}

#[repr(C)]
pub enum PriceStatus
{
  Unknown,
  Trading,
  Halted,
  Auction
}

#[repr(C)]
pub enum CorpAction
{
  NoCorpAct
}

#[repr(C)]
pub struct PriceInfo
{
  pub price      : i64,
  pub conf       : u64,
  pub status     : PriceStatus,
  pub corp_act   : CorpAction,
  pub pub_slot   : u64
}

#[repr(C)]
pub struct PriceComp
{
  publisher  : AccKey,
  agg        : PriceInfo,
  latest     : PriceInfo
}

#[repr(C)]
pub enum PriceType
{
  Unknown,
  Price,
  TWAP,
  Volatility
}

#[repr(C)]
pub struct Price
{
  pub magic      : u32,        // pyth magic number
  pub ver        : u32,        // program version
  pub atype      : u32,        // account type
  pub size       : u32,        // price account size
  pub ptype      : PriceType,  // price or calculation type
  pub expo       : i32,        // price exponent
  pub num        : u32,        // number of component prices
  pub unused     : u32,
  pub curr_slot  : u64,        // currently accumulating price slot
  pub valid_slot : u64,        // valid slot-time of agg. price
  pub prod       : AccKey,
  pub next       : AccKey,
  pub agg_pub    : AccKey,
  pub agg        : PriceInfo,
  pub comp       : [PriceComp;16]
}

struct AccKeyU64
{
  pub val: [u64;4]
}

pub fn cast<T>( d: &[u8] ) -> &T {
  let (_, pxa, _) = unsafe { d.align_to::<T>() };
  &pxa[0]
}

impl AccKey
{
  pub fn is_valid( &self ) -> bool  {
    let k8 = cast::<AccKeyU64>( &self.val );
    return k8.val[0]!=0 || k8.val[1]!=0 || k8.val[2]!=0 || k8.val[3]!=0;
  }
}


