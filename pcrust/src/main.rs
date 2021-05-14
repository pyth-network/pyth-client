// example usage of pyth-client account structure
// bootstrap all product and pricing accounts from root mapping account

use pyth_client::{
  AccountType,
  Mapping,
  Product,
  Price,
  PriceType,
  PriceStatus,
  CorpAction,
  cast,
  MAGIC,
  VERSION_1,
  PROD_HDR_SIZE
};
use solana_client::{
  rpc_client::RpcClient
};
use solana_program::{
  pubkey::Pubkey
};
use std::{
  str::FromStr
};

fn get_attr_str<'a,T>( ite: & mut T ) -> String
where T : Iterator<Item=& 'a u8>
{
  let mut len = *ite.next().unwrap() as usize;
  let mut val = String::with_capacity( len );
  while len > 0 {
    val.push( *ite.next().unwrap() as char );
    len -= 1;
  }
  return val
}

fn get_price_type( ptype: &PriceType ) -> &'static str
{
  match ptype {
    PriceType::Unknown    => "unknown",
    PriceType::Price      => "price",
    PriceType::TWAP       => "twap",
    PriceType::Volatility => "volatility",
  }
}

fn get_status( st: &PriceStatus ) -> &'static str
{
  match st {
    PriceStatus::Unknown => "unknown",
    PriceStatus::Trading => "trading",
    PriceStatus::Halted  => "halted",
    PriceStatus::Auction => "auction",
  }
}

fn get_corp_act( cact: &CorpAction ) -> &'static str
{
  match cact {
    CorpAction::NoCorpAct => "nocorpact",
  }
}

fn main() {
  // get pyth mapping account
  let url = "http://devnet.solana.com:8899";
  let key = "ArppEFcsybCLE8CRtQJLQ9tLv2peGmQoKWFuiUWm4KBP";
  let clnt = RpcClient::new( url.to_string() );
  let mut akey = Pubkey::from_str( key ).unwrap();

  loop {
    // get Mapping account from key
    let map_data = clnt.get_account_data( &akey ).unwrap();
    let map_acct = cast::<Mapping>( &map_data );
    assert_eq!( map_acct.magic, MAGIC, "not a valid pyth account" );
    assert_eq!( map_acct.atype, AccountType::Mapping as u32,
                "not a valid pyth mapping account" );
    assert_eq!( map_acct.ver, VERSION_1,
                "unexpected pyth mapping account version" );

    // iget and print each Product in Mapping directory
    let mut i = 0;
    for prod_akey in &map_acct.products {
      let prod_pkey = Pubkey::new( &prod_akey.val );
      let prod_data = clnt.get_account_data( &prod_pkey ).unwrap();
      let prod_acct = cast::<Product>( &prod_data );
      assert_eq!( prod_acct.magic, MAGIC, "not a valid pyth account" );
      assert_eq!( prod_acct.atype, AccountType::Product as u32,
                  "not a valid pyth product account" );
      assert_eq!( prod_acct.ver, VERSION_1,
                  "unexpected pyth product account version" );

      // print key and reference data for this Product
      println!( "product_account .. {:?}", prod_pkey );
      let mut psz = prod_acct.size as usize - PROD_HDR_SIZE;
      let mut pit = (&prod_acct.attr[..]).iter();
      while psz > 0 {
        let key = get_attr_str( &mut pit );
        let val = get_attr_str( &mut pit );
        println!( "  {:.<16} {}", key, val );
        psz -= 2 + key.len() + val.len();
      }

      // print all Prices that correspond to this Product
      if prod_acct.px_acc.is_valid() {
        let mut px_pkey = Pubkey::new( &prod_acct.px_acc.val );
        loop {
          let pd = clnt.get_account_data( &px_pkey ).unwrap();
          let pa = cast::<Price>( &pd );
          assert_eq!( pa.magic, MAGIC, "not a valid pyth account" );
          assert_eq!( pa.atype, AccountType::Price as u32,
                     "not a valid pyth price account" );
          assert_eq!( pa.ver, VERSION_1,
                      "unexpected pyth price account version" );
          println!( "  price_account .. {:?}", px_pkey );
          println!( "    price_type ... {}", get_price_type(&pa.ptype));
          println!( "    exponent ..... {}", pa.expo );
          println!( "    status ....... {}", get_status(&pa.agg.status));
          println!( "    corp_act ..... {}", get_corp_act(&pa.agg.corp_act));
          println!( "    price ........ {}", pa.agg.price );
          println!( "    conf ......... {}", pa.agg.conf );
          println!( "    valid_slot ... {}", pa.valid_slot );
          println!( "    publish_slot . {}", pa.agg.pub_slot );

          // go to next price account in list
          if pa.next.is_valid() {
            px_pkey = Pubkey::new( &pa.next.val );
          } else {
            break;
          }
        }
      }
      // go to next product
      i += 1;
      if i == map_acct.num {
        break;
      }
    }

    // go to next Mapping account in list
    if !map_acct.next.is_valid() {
      break;
    }
    akey = Pubkey::new( &map_acct.next.val );
  }
}
