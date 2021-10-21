#include "admin_rpc_client.hpp"
#include <pc/bincode.hpp>

using namespace pc;

// system program instructions
enum system_instruction : uint32_t
{
  e_create_account,
  e_assign,
};

// system program id
static hash gen_sys_id()
{
  pc::hash id;
  id.zero();
  return id;
}
static hash sys_id = gen_sys_id();

// generate json for sendTransaction
static void send_transaction( json_wtr& msg, bincode& tx )
{
  msg.add_key( "method", "sendTransaction" );
  msg.add_key( "params", json_wtr::e_arr );
  msg.add_val_enc_base64( str( tx.get_buf(), tx.size() ) );
  msg.add_val( json_wtr::e_obj );
  msg.add_key( "encoding", "base64" );
  msg.pop();
  msg.pop();
}

///////////////////////////////////////////////////////////////////////////
// rpc_request

template<class T>
void rpc_request::on_response( T *req )
{
  req->set_recv_time( get_now() );
  rpc_sub_i<T> *iptr = dynamic_cast<rpc_sub_i<T>*>( req->get_sub() );
  if ( iptr ) {
    iptr->on_response( req );
  }
}

template<class T>
bool rpc_request::on_error( const jtree& jt, T *req )
{
  uint32_t etok = jt.find_val( 1, "error" );
  if ( etok == 0 ) return false;
  const char *txt = nullptr;
  size_t txt_len = 0;
  std::string emsg;
  jt.get_text( jt.find_val( etok, "message" ), txt, txt_len );
  emsg.assign( txt, txt_len );
  set_err_msg( emsg );
  set_err_code( jt.get_int( jt.find_val( etok, "code" ) ) );
  on_response( req );
  return true;
}

///////////////////////////////////////////////////////////////////////////
// get_minimum_balance_rent_exemption

rpc::get_minimum_balance_rent_exemption::get_minimum_balance_rent_exemption()
: sz_( 128 ),
  lamports_( 0 )
{
}

void rpc::get_minimum_balance_rent_exemption::set_size( size_t sz )
{
  sz_ = sz;
}

uint64_t rpc::get_minimum_balance_rent_exemption::get_lamports() const
{
  return lamports_;
}

void rpc::get_minimum_balance_rent_exemption::request( json_wtr& msg )
{
  msg.add_key( "method", "getMinimumBalanceForRentExemption" );
  msg.add_key( "params", json_wtr::e_arr );
  msg.add_val( sz_ );
  msg.pop();
}

void rpc::get_minimum_balance_rent_exemption::response( const jtree& jt)
{
  if ( on_error( jt, this ) ) return;

  lamports_ = jt.get_uint( jt.find_val( 1, "result" ) );
  on_response( this );
}

///////////////////////////////////////////////////////////////////////////
// create_account

void rpc::create_account::set_block_hash( hash *bhash )
{
  bhash_ = bhash;
}

void rpc::create_account::set_sender( key_pair *snd )
{
  snd_ = snd;
}

void rpc::create_account::set_account( key_pair *acc )
{
  account_ = acc;
}

void rpc::create_account::set_owner( pub_key *owner )
{
  owner_ = owner;
}

void rpc::create_account::set_lamports( uint64_t funds )
{
  lamports_ = funds;
}

void rpc::create_account::set_space( uint64_t num_bytes )
{
  space_ = num_bytes;
}

signature *rpc::create_account::get_signature()
{
  return &sig_;
}

rpc::create_account::create_account()
: lamports_( 0L ),
  space_( 0L )
{
}

void rpc::create_account::request( json_wtr& msg )
{
  // construct binary transaction
  net_buf *bptr = net_buf::alloc();
  bincode tx( bptr->buf_ );

  // signatures section
  tx.add_len<2>();      // two signatures (funding and account)
  size_t fund_idx = tx.reserve_sign();
  size_t acct_idx = tx.reserve_sign();

  // message header
  size_t tx_idx = tx.get_pos();
  tx.add( (uint8_t)2 ); // funding and new account are signing accounts
  tx.add( (uint8_t)0 ); // read-only signed accounts
  tx.add( (uint8_t)1 ); // program-id are read-only unsigned accounts

  // accounts
  tx.add_len<3>();      // 3 accounts: sender, account, program
  tx.add( *snd_ );      // sender/funding account
  tx.add( *account_ );  // account being created
  tx.add( sys_id );     // system programid

  // recent block hash
  tx.add( *bhash_ );    // recent block hash

  // instructions section
  tx.add_len<1>();      // one instruction
  tx.add( (uint8_t)2);  // program_id index
  tx.add_len<2>();      // 2 accounts: sender, new account
  tx.add( (uint8_t)0 ); // index of sender account
  tx.add( (uint8_t)1 ); // index of new account

  // instruction parameter section
  tx.add_len<52>();     // size of data array
  tx.add( (uint32_t)system_instruction::e_create_account );
  tx.add( (uint64_t)lamports_ );
  tx.add( (uint64_t)space_ );
  tx.add( *owner_ );

  // both funding and new account sign message
  tx.sign( fund_idx, tx_idx, *snd_ );
  sig_.init_from_buf( (const uint8_t*)(tx.get_buf() + fund_idx) );
  tx.sign( acct_idx, tx_idx, *account_ );

  // encode transaction and add to json params
  send_transaction( msg, tx );
  bptr->dealloc();
}

void rpc::create_account::response( const jtree& jt )
{
  if ( on_error( jt, this ) ) return;
  on_response( this );
}

///////////////////////////////////////////////////////////////////////////
// signature_subscribe

rpc::signature_subscribe::signature_subscribe()
: cmt_( commitment::e_finalized ),
  slot_( 0UL )
{
}

void rpc::signature_subscribe::set_signature( signature *sig )
{
  sig_ = sig;
}

void rpc::signature_subscribe::set_commitment( commitment val )
{
  cmt_ = val;
}

uint64_t rpc::signature_subscribe::get_slot() const
{
  return slot_;
}

void rpc::signature_subscribe::request( json_wtr& msg )
{
  msg.add_key( "method", "signatureSubscribe" );
  msg.add_key( "params", json_wtr::e_arr );
  msg.add_val( *sig_ );
  msg.add_val( json_wtr::e_obj );
  msg.add_key( "commitment", commitment_to_str( cmt_ ) );
  msg.pop();
  msg.pop();
}

void rpc::signature_subscribe::response( const jtree& jt )
{
  if ( on_error( jt, this ) ) return;

  // add to notification list
  add_notify( jt );
}

bool rpc::signature_subscribe::notify( const jtree& jt )
{
  if ( on_error( jt, this ) ) return true;

  uint32_t ptok = jt.find_val( 1, "params" );
  uint32_t rtok = jt.find_val( ptok, "result" );
  uint32_t ctok = jt.find_val( rtok, "context" );
  slot_ = jt.get_uint( jt.find_val( ctok, "slot" ) );

  on_response( this );
  return true;  // remove notification
}

///////////////////////////////////////////////////////////////////////////
// init_mapping

void rpc::init_mapping::set_block_hash( hash* bhash )
{
  bhash_ = bhash;
}

void rpc::init_mapping::set_publish( key_pair *kp )
{
  pkey_ = kp;
}

void rpc::init_mapping::set_mapping( key_pair *kp )
{
  mkey_ = kp;
}

void rpc::init_mapping::set_program( pub_key *gkey )
{
  gkey_ = gkey;
}

signature *rpc::init_mapping::get_signature()
{
  return &sig_;
}

void rpc::init_mapping::request( json_wtr& msg )
{
  // construct binary transaction
  net_buf *bptr = net_buf::alloc();
  bincode tx( bptr->buf_ );

  // signatures section
  tx.add_len<2>();      // two signatures (funding and account)
  size_t pub_idx = tx.reserve_sign();
  size_t map_idx = tx.reserve_sign();

  // message header
  size_t tx_idx = tx.get_pos();
  tx.add( (uint8_t)2 ); // funding and mapping account are signing accounts
  tx.add( (uint8_t)0 ); // read-only signed accounts
  tx.add( (uint8_t)1 ); // program-id are read-only unsigned accounts

  // accounts
  tx.add_len<3>();      // 3 accounts: publish, mapping, program
  tx.add( *pkey_ );     // publish account
  tx.add( *mkey_ );     // mapping account
  tx.add( *gkey_ );     // programid

  // recent block hash
  tx.add( *bhash_ );     // recent block hash

  // instructions section
  tx.add_len<1>();      // one instruction
  tx.add( (uint8_t)2);  // program_id index
  tx.add_len<2>();      // 2 accounts: mapping, publish
  tx.add( (uint8_t)0 ); // index of publish account
  tx.add( (uint8_t)1 ); // index of mapping account

  // instruction parameter section
  tx.add_len<sizeof(cmd_hdr)>();     // size of data array
  tx.add( (uint32_t)PC_VERSION );
  tx.add( (uint32_t)e_cmd_init_mapping );

  // both publish and mapping sign message
  tx.sign( pub_idx, tx_idx, *pkey_ );
  sig_.init_from_buf( (const uint8_t*)(tx.get_buf() + pub_idx) );
  tx.sign( map_idx, tx_idx, *mkey_ );

  // encode transaction and add to json params
  send_transaction( msg, tx );
  bptr->dealloc();
}

void rpc::init_mapping::response( const jtree& jt )
{
  if ( on_error( jt, this ) ) return;
  on_response( this );
}

///////////////////////////////////////////////////////////////////////////
// add_mapping

void rpc::add_mapping::set_block_hash( hash* bhash )
{
  bhash_ = bhash;
}

void rpc::add_mapping::set_publish( key_pair *kp )
{
  pkey_ = kp;
}

void rpc::add_mapping::set_mapping( key_pair *kp )
{
  mkey_ = kp;
}

void rpc::add_mapping::set_account( key_pair *kp )
{
  akey_ = kp;
}

void rpc::add_mapping::set_program( pub_key *gkey )
{
  gkey_ = gkey;
}

signature *rpc::add_mapping::get_signature()
{
  return &sig_;
}

void rpc::add_mapping::request( json_wtr& msg )
{
  // construct binary transaction
  net_buf *bptr = net_buf::alloc();
  bincode tx( bptr->buf_ );

  // signatures section
  tx.add_len<3>();      // three signatures (funding, mapping and account)
  size_t pub_idx = tx.reserve_sign();
  size_t map_idx = tx.reserve_sign();
  size_t acc_idx = tx.reserve_sign();

  // message header
  size_t tx_idx = tx.get_pos();
  tx.add( (uint8_t)3 ); // fund, map and new map are signing accounts
  tx.add( (uint8_t)0 ); // read-only signed accounts
  tx.add( (uint8_t)1 ); // program-id are read-only unsigned accounts

  // accounts
  tx.add_len<4>();      // 4 accounts: publish, mapping, new-account, program
  tx.add( *pkey_ );     // publish account
  tx.add( *mkey_ );     // mapping account
  tx.add( *akey_ );     // new mapping account
  tx.add( *gkey_ );     // programid

  // recent block hash
  tx.add( *bhash_ );     // recent block hash

  // instructions section
  tx.add_len<1>();      // one instruction
  tx.add( (uint8_t)3);  // program_id index
  tx.add_len<3>();      // 3 accounts: publish, mapping and new account
  tx.add( (uint8_t)0 ); // index of publish account
  tx.add( (uint8_t)1 ); // index of mapping account
  tx.add( (uint8_t)2 ); // index of new account

  // instruction parameter section
  tx.add_len<sizeof(cmd_hdr)>();     // size of data array
  tx.add( (uint32_t)PC_VERSION );
  tx.add( (uint32_t)e_cmd_add_mapping );

  // all accounts need to sign transaction
  tx.sign( pub_idx, tx_idx, *pkey_ );
  sig_.init_from_buf( (const uint8_t*)(tx.get_buf() + pub_idx) );
  tx.sign( map_idx, tx_idx, *mkey_ );
  tx.sign( acc_idx, tx_idx, *akey_ );

  // encode transaction and add to json params
  send_transaction( msg, tx );
  bptr->dealloc();
}

void rpc::add_mapping::response( const jtree& jt )
{
  if ( on_error( jt, this ) ) return;
  on_response( this );
}

///////////////////////////////////////////////////////////////////////////
// add_product

void rpc::add_product::set_program( pub_key *gkey )
{
  gkey_ = gkey;
}

void rpc::add_product::set_block_hash( hash *bhash )
{
  bhash_ = bhash;
}

void rpc::add_product::set_publish( key_pair *pkey )
{
  pkey_ = pkey;
}

void rpc::add_product::set_account( key_pair *akey )
{
  akey_ = akey;
}

void rpc::add_product::set_mapping( key_pair *mkey )
{
  mkey_ = mkey;
}

signature *rpc::add_product::get_signature()
{
  return &sig_;
}

rpc::add_product::add_product()
: bhash_( nullptr ),
  pkey_( nullptr ),
  gkey_( nullptr ),
  akey_( nullptr ),
  mkey_( nullptr )
{
}

void rpc::add_product::request( json_wtr& msg )
{
  // construct binary transaction
  net_buf *bptr = net_buf::alloc();
  bincode tx( bptr->buf_ );

  // signatures section
  tx.add_len<3>();      // three signatures (publish, symbol, mapping)
  size_t pub_idx = tx.reserve_sign();
  size_t map_idx = tx.reserve_sign();
  size_t sym_idx = tx.reserve_sign();

  // message header
  size_t tx_idx = tx.get_pos();
  tx.add( (uint8_t)3 ); // pub, symbol and mapping are signing accounts
  tx.add( (uint8_t)0 ); // read-only signed accounts
  tx.add( (uint8_t)1 ); // program-id are read-only unsigned accounts

  // accounts
  tx.add_len<4>();      // 4 accounts: publish, mapping, product, program
  tx.add( *pkey_ );     // publish account
  tx.add( *mkey_ );     // mapping account
  tx.add( *akey_ );     // product account
  tx.add( *gkey_ );     // programid

  // recent block hash
  tx.add( *bhash_ );    // recent block hash

  // instructions section
  tx.add_len<1>();      // one instruction
  tx.add( (uint8_t)3);  // program_id index
  tx.add_len<3>();      // 3 accounts: publish, mapping, product
  tx.add( (uint8_t)0 ); // index of publish account
  tx.add( (uint8_t)1 ); // index of mapping account
  tx.add( (uint8_t)2 ); // index of product account

  // instruction parameter section
  tx.add_len<sizeof(cmd_add_product)>();
  tx.add( (uint32_t)PC_VERSION );
  tx.add( (int32_t)e_cmd_add_product );

  // all accounts need to sign transaction
  tx.sign( pub_idx, tx_idx, *pkey_ );
  sig_.init_from_buf( (const uint8_t*)(tx.get_buf() + pub_idx) );
  tx.sign( map_idx, tx_idx, *mkey_ );
  tx.sign( sym_idx, tx_idx, *akey_ );

  // encode transaction and add to json params
  send_transaction( msg, tx );
  bptr->dealloc();
}

void rpc::add_product::response( const jtree& jt )
{
  if ( on_error( jt, this ) ) return;
  on_response( this );
}

///////////////////////////////////////////////////////////////////////////
// upd_product

void rpc::upd_product::set_program( pub_key *gkey )
{
  gkey_ = gkey;
}

void rpc::upd_product::set_block_hash( hash *bhash )
{
  bhash_ = bhash;
}

void rpc::upd_product::set_publish( key_pair *pkey )
{
  pkey_ = pkey;
}

void rpc::upd_product::set_account( key_pair *akey )
{
  akey_ = akey;
}

void rpc::upd_product::set_attr_dict( attr_dict *aptr )
{
  aptr_ = aptr;
}

signature *rpc::upd_product::get_signature()
{
  return &sig_;
}

rpc::upd_product::upd_product()
: bhash_( nullptr ),
  pkey_( nullptr ),
  gkey_( nullptr ),
  akey_( nullptr ),
  aptr_( nullptr )
{
}

void rpc::upd_product::request( json_wtr& msg )
{
  // construct binary transaction
  net_buf *bptr = net_buf::alloc();
  bincode tx( bptr->buf_ );

  // signatures section
  tx.add_len<2>();      // three signatures (publish, product)
  size_t pub_idx = tx.reserve_sign();
  size_t sym_idx = tx.reserve_sign();

  // message header
  size_t tx_idx = tx.get_pos();
  tx.add( (uint8_t)2 ); // pub, and product are signing accounts
  tx.add( (uint8_t)0 ); // read-only signed accounts
  tx.add( (uint8_t)1 ); // program-id are read-only unsigned accounts

  // accounts
  tx.add_len<3>();      // 3 accounts: publish, product, program
  tx.add( *pkey_ );     // publish account
  tx.add( *akey_ );     // product account
  tx.add( *gkey_ );     // programid

  // recent block hash
  tx.add( *bhash_ );    // recent block hash

  // instructions section
  tx.add_len<1>();      // one instruction
  tx.add( (uint8_t)2);  // program_id index
  tx.add_len<2>();      // 2 accounts: publish, product
  tx.add( (uint8_t)0 ); // index of publish account
  tx.add( (uint8_t)1 ); // index of product account

  // serialize attribute dictionary and copy to transaction
  net_wtr awtr;
  aptr_->write_account( awtr );
  net_buf *ahd, *atl;
  awtr.detach( ahd, atl );

  // instruction parameter section
  tx.add_len( sizeof(cmd_upd_product) + ahd->size_ );
  tx.add( (uint32_t)PC_VERSION );
  tx.add( (int32_t)e_cmd_upd_product );
  tx.add( ahd->buf_, ahd->size_ );
  ahd->dealloc();

  // all accounts need to sign transaction
  tx.sign( pub_idx, tx_idx, *pkey_ );
  sig_.init_from_buf( (const uint8_t*)(tx.get_buf() + pub_idx) );
  tx.sign( sym_idx, tx_idx, *akey_ );

  // encode transaction and add to json params
  send_transaction( msg, tx );
  bptr->dealloc();
}

void rpc::upd_product::response( const jtree& jt )
{
  if ( on_error( jt, this ) ) return;
  on_response( this );
}

///////////////////////////////////////////////////////////////////////////
// add_price

price_type rpc::add_price::get_price_type() const
{
  return ptype_;
}

void rpc::add_price::set_exponent( int32_t expo )
{
  expo_ = expo;
}

void rpc::add_price::set_price_type( price_type ptype )
{
  ptype_ = ptype;
}

void rpc::add_price::set_program( pub_key *gkey )
{
  gkey_ = gkey;
}

void rpc::add_price::set_block_hash( hash *bhash )
{
  bhash_ = bhash;
}

void rpc::add_price::set_publish( key_pair *pkey )
{
  pkey_ = pkey;
}

void rpc::add_price::set_account( key_pair *akey )
{
  akey_ = akey;
}

void rpc::add_price::set_product( key_pair *mkey )
{
  skey_ = mkey;
}

signature *rpc::add_price::get_signature()
{
  return &sig_;
}

rpc::add_price::add_price()
: expo_( 0 ),
  ptype_( price_type::e_price ),
  bhash_( nullptr ),
  pkey_( nullptr ),
  gkey_( nullptr ),
  akey_( nullptr ),
  skey_( nullptr )
{
}

void rpc::add_price::request( json_wtr& msg )
{
  // construct binary transaction
  net_buf *bptr = net_buf::alloc();
  bincode tx( bptr->buf_ );

  // signatures section
  tx.add_len<3>();      // three signatures (publish, product, price)
  size_t pub_idx = tx.reserve_sign();
  size_t prd_idx = tx.reserve_sign();
  size_t prc_idx = tx.reserve_sign();

  // message header
  size_t tx_idx = tx.get_pos();
  tx.add( (uint8_t)3 ); // pub, product and price are signing accounts
  tx.add( (uint8_t)0 ); // read-only signed accounts
  tx.add( (uint8_t)1 ); // program-id are read-only unsigned accounts

  // accounts
  tx.add_len<4>();      // 4 accounts: publish, product, price, program
  tx.add( *pkey_ );     // publish account
  tx.add( *skey_ );     // product account
  tx.add( *akey_ );     // price account
  tx.add( *gkey_ );     // programid

  // recent block hash
  tx.add( *bhash_ );    // recent block hash

  // instructions section
  tx.add_len<1>();      // one instruction
  tx.add( (uint8_t)3);  // program_id index
  tx.add_len<3>();      // 3 accounts: publish, product, price
  tx.add( (uint8_t)0 ); // index of publish account
  tx.add( (uint8_t)1 ); // index of product account
  tx.add( (uint8_t)2 ); // index of price account

  // instruction parameter section
  tx.add_len<sizeof(cmd_add_price)>();
  tx.add( (uint32_t)PC_VERSION );
  tx.add( (int32_t)e_cmd_add_price );
  tx.add( (int32_t)expo_ );
  tx.add( (uint32_t)ptype_ );

  // all accounts need to sign transaction
  tx.sign( pub_idx, tx_idx, *pkey_ );
  sig_.init_from_buf( (const uint8_t*)(tx.get_buf() + pub_idx) );
  tx.sign( prd_idx, tx_idx, *skey_ );
  tx.sign( prc_idx, tx_idx, *akey_ );

  // encode transaction and add to json params
  send_transaction( msg, tx );
  bptr->dealloc();
}

void rpc::add_price::response( const jtree& jt )
{
  if ( on_error( jt, this ) ) return;
  on_response( this );
}

///////////////////////////////////////////////////////////////////////////
// init_price

price_type rpc::init_price::get_price_type() const
{
  return ptype_;
}

void rpc::init_price::set_exponent( int32_t expo )
{
  expo_ = expo;
}

void rpc::init_price::set_price_type( price_type ptype )
{
  ptype_ = ptype;
}

void rpc::init_price::set_program( pub_key *gkey )
{
  gkey_ = gkey;
}

void rpc::init_price::set_block_hash( hash *bhash )
{
  bhash_ = bhash;
}

void rpc::init_price::set_publish( key_pair *pkey )
{
  pkey_ = pkey;
}

void rpc::init_price::set_account( key_pair *akey )
{
  akey_ = akey;
}

signature *rpc::init_price::get_signature()
{
  return &sig_;
}

rpc::init_price::init_price()
: expo_( 0 ),
  ptype_( price_type::e_price ),
  bhash_( nullptr ),
  pkey_( nullptr ),
  gkey_( nullptr ),
  akey_( nullptr )
{
}

void rpc::init_price::request( json_wtr& msg )
{
  // construct binary transaction
  net_buf *bptr = net_buf::alloc();
  bincode tx( bptr->buf_ );

  // signatures section
  tx.add_len<2>();      // three signatures (publish, price)
  size_t pub_idx = tx.reserve_sign();
  size_t prc_idx = tx.reserve_sign();

  // message header
  size_t tx_idx = tx.get_pos();
  tx.add( (uint8_t)2 ); // pub and price signing accounts
  tx.add( (uint8_t)0 ); // read-only signed accounts
  tx.add( (uint8_t)1 ); // program-id are read-only unsigned accounts

  // accounts
  tx.add_len<3>();      // 3 accounts: publish, price, params, program
  tx.add( *pkey_ );     // publish account
  tx.add( *akey_ );     // price account
  tx.add( *gkey_ );     // programid

  // recent block hash
  tx.add( *bhash_ );    // recent block hash

  // instructions section
  tx.add_len<1>();      // one instruction
  tx.add( (uint8_t)2);  // program_id index
  tx.add_len<2>();      // 2 accounts: publish, price
  tx.add( (uint8_t)0 ); // index of publish account
  tx.add( (uint8_t)1 ); // index of price account

  // instruction parameter section
  tx.add_len<sizeof(cmd_init_price)>();
  tx.add( (uint32_t)PC_VERSION );
  tx.add( (int32_t)e_cmd_init_price );
  tx.add( (int32_t)expo_ );
  tx.add( (uint32_t)ptype_ );

  // all accounts need to sign transaction
  tx.sign( pub_idx, tx_idx, *pkey_ );
  sig_.init_from_buf( (const uint8_t*)(tx.get_buf() + pub_idx) );
  tx.sign( prc_idx, tx_idx, *akey_ );

  // encode transaction and add to json params
  send_transaction( msg, tx );
  bptr->dealloc();
}

void rpc::init_price::response( const jtree& jt )
{
  if ( on_error( jt, this ) ) return;
  on_response( this );
}

///////////////////////////////////////////////////////////////////////////
// add_publisher

void rpc::add_publisher::set_program( pub_key *gkey )
{
  gkey_ = gkey;
}

void rpc::add_publisher::set_block_hash( hash *bhash )
{
  bhash_ = bhash;
}

void rpc::add_publisher::set_publish( key_pair *pkey )
{
  pkey_ = pkey;
}

void rpc::add_publisher::set_account( key_pair *akey )
{
  akey_ = akey;
}

void rpc::add_publisher::set_publisher( const pub_key& nkey )
{
  nkey_ = nkey;
}

pub_key *rpc::add_publisher::get_publisher()
{
  return &nkey_;
}

signature *rpc::add_publisher::get_signature()
{
  return &sig_;
}

void rpc::add_publisher::request( json_wtr& msg )
{
  // construct binary transaction
  net_buf *bptr = net_buf::alloc();
  bincode tx( bptr->buf_ );

  // signatures section
  tx.add_len<2>();      // two signatures (publish, price )
  size_t pub_idx = tx.reserve_sign();
  size_t sym_idx = tx.reserve_sign();

  // message header
  size_t tx_idx = tx.get_pos();
  tx.add( (uint8_t)2 ); // pub and  symbol are signing accounts
  tx.add( (uint8_t)0 ); // read-only signed accounts
  tx.add( (uint8_t)1 ); // program-id are read-only unsigned accounts

  // accounts
  tx.add_len<3>();      // 3 accounts: publish, symbol, program
  tx.add( *pkey_ );     // publish account
  tx.add( *akey_ );     // symbol account
  tx.add( *gkey_ );     // programid

  // recent block hash
  tx.add( *bhash_ );    // recent block hash

  // instructions section
  tx.add_len<1>();      // one instruction
  tx.add( (uint8_t)2);  // program_id index
  tx.add_len<2>();      // 2 accounts: publish, symbol
  tx.add( (uint8_t)0 ); // index of publish account
  tx.add( (uint8_t)1 ); // index of symbol account

  // instruction parameter section
  tx.add_len<sizeof(cmd_add_publisher)>();
  tx.add( (uint32_t)PC_VERSION );
  tx.add( (int32_t)e_cmd_add_publisher );
  tx.add( nkey_ );

  // all accounts need to sign transaction
  tx.sign( pub_idx, tx_idx, *pkey_ );
  sig_.init_from_buf( (const uint8_t*)(tx.get_buf() + pub_idx) );
  tx.sign( sym_idx, tx_idx, *akey_ );

  // encode transaction and add to json params
  send_transaction( msg, tx );
  bptr->dealloc();
}

void rpc::add_publisher::response( const jtree& jt )
{
  if ( on_error( jt, this ) ) return;
  on_response( this );
}

///////////////////////////////////////////////////////////////////////////
// del_publisher

void rpc::del_publisher::set_program( pub_key *gkey )
{
  gkey_ = gkey;
}

void rpc::del_publisher::set_block_hash( hash *bhash )
{
  bhash_ = bhash;
}

void rpc::del_publisher::set_publish( key_pair *pkey )
{
  pkey_ = pkey;
}

void rpc::del_publisher::set_account( key_pair *akey )
{
  akey_ = akey;
}

void rpc::del_publisher::set_publisher( const pub_key& nkey )
{
  nkey_ = nkey;
}

pub_key *rpc::del_publisher::get_publisher()
{
  return &nkey_;
}

signature *rpc::del_publisher::get_signature()
{
  return &sig_;
}

void rpc::del_publisher::request( json_wtr& msg )
{
  // construct binary transaction
  net_buf *bptr = net_buf::alloc();
  bincode tx( bptr->buf_ );

  // signatures section
  tx.add_len<2>();      // two signatures (publish, price )
  size_t pub_idx = tx.reserve_sign();
  size_t sym_idx = tx.reserve_sign();

  // message header
  size_t tx_idx = tx.get_pos();
  tx.add( (uint8_t)2 ); // pub and  symbol are signing accounts
  tx.add( (uint8_t)0 ); // read-only signed accounts
  tx.add( (uint8_t)1 ); // program-id are read-only unsigned accounts

  // accounts
  tx.add_len<3>();      // 3 accounts: publish, symbol, program
  tx.add( *pkey_ );     // publish account
  tx.add( *akey_ );     // symbol account
  tx.add( *gkey_ );     // programid

  // recent block hash
  tx.add( *bhash_ );    // recent block hash

  // instructions section
  tx.add_len<1>();      // one instruction
  tx.add( (uint8_t)2);  // program_id index
  tx.add_len<2>();      // 2 accounts: publish, symbol
  tx.add( (uint8_t)0 ); // index of publish account
  tx.add( (uint8_t)1 ); // index of symbol account

  // instruction parameter section
  tx.add_len<sizeof(cmd_del_publisher)>();
  tx.add( (uint32_t)PC_VERSION );
  tx.add( (int32_t)e_cmd_del_publisher );
  tx.add( nkey_ );

  // all accounts need to sign transaction
  tx.sign( pub_idx, tx_idx, *pkey_ );
  sig_.init_from_buf( (const uint8_t*)(tx.get_buf() + pub_idx) );
  tx.sign( sym_idx, tx_idx, *akey_ );

  // encode transaction and add to json params
  send_transaction( msg, tx );
  bptr->dealloc();
}

void rpc::del_publisher::response( const jtree& jt )
{
  if ( on_error( jt, this ) ) return;
  on_response( this );
}

///////////////////////////////////////////////////////////////////////////
// init_test

void rpc::init_test::set_block_hash( hash* bhash )
{
  bhash_ = bhash;
}

void rpc::init_test::set_publish( key_pair *kp )
{
  pkey_ = kp;
}

void rpc::init_test::set_account( key_pair *kp )
{
  akey_ = kp;
}

void rpc::init_test::set_program( pub_key *gkey )
{
  gkey_ = gkey;
}

signature *rpc::init_test::get_signature()
{
  return &sig_;
}

void rpc::init_test::request( json_wtr& msg )
{
  // construct binary transaction
  net_buf *bptr = net_buf::alloc();
  bincode tx( bptr->buf_ );

  // signatures section
  tx.add_len<2>();      // two signatures (funding and account)
  size_t pub_idx = tx.reserve_sign();
  size_t prm_idx = tx.reserve_sign();

  // message header
  size_t tx_idx = tx.get_pos();
  tx.add( (uint8_t)2 ); // funding and test account are signing accounts
  tx.add( (uint8_t)0 ); // read-only signed accounts
  tx.add( (uint8_t)1 ); // program-id are read-only unsigned accounts

  // accounts
  tx.add_len<3>();      // 3 accounts: publish, test, program
  tx.add( *pkey_ );     // publish account
  tx.add( *akey_ );     // test account
  tx.add( *gkey_ );     // programid

  // recent block hash
  tx.add( *bhash_ );     // recent block hash

  // instructions section
  tx.add_len<1>();      // one instruction
  tx.add( (uint8_t)2);  // program_id index
  tx.add_len<2>();      // 2 accounts: publish and test
  tx.add( (uint8_t)0 ); // index of publish account
  tx.add( (uint8_t)1 ); // index of param account

  // instruction parameter section
  tx.add_len<sizeof(cmd_hdr)>();     // size of data array
  tx.add( (uint32_t)PC_VERSION );
  tx.add( (uint32_t)e_cmd_init_test );

  // both publish and param sign message
  tx.sign( pub_idx, tx_idx, *pkey_ );
  sig_.init_from_buf( (const uint8_t*)(tx.get_buf() + pub_idx) );
  tx.sign( prm_idx, tx_idx, *akey_ );

  // encode transaction and add to json params
  send_transaction( msg, tx );
  bptr->dealloc();
}

void rpc::init_test::response( const jtree& jt )
{
  if ( on_error( jt, this ) ) return;
  on_response( this );
}

///////////////////////////////////////////////////////////////////////////
// upd_test

rpc::upd_test::upd_test()
{
  __builtin_memset( upd_, 0, sizeof( upd_ ) );
  upd_->ver_ = PC_VERSION;
  upd_->cmd_ = e_cmd_upd_test;
}

void rpc::upd_test::set_block_hash( hash *bhash )
{
  bhash_ = bhash;
}

void rpc::upd_test::set_publish( key_pair *pkey )
{
  pkey_ = pkey;
}

void rpc::upd_test::set_account( key_pair *tkey )
{
  tkey_ = tkey;
}

void rpc::upd_test::set_program( pub_key *gkey )
{
  gkey_ = gkey;
}

void rpc::upd_test::set_expo( int expo )
{
  upd_->expo_ = expo;
}

void rpc::upd_test::set_num( uint32_t num )
{
  upd_->num_ = num;
}

void rpc::upd_test::set_price( unsigned i,
                               int64_t px,
                               uint64_t conf,
                               int64_t sdiff )
{
  upd_->price_[i] = px;
  upd_->conf_[i] = conf;
  upd_->slot_diff_[i] = (int8_t)sdiff;
}

signature *rpc::upd_test::get_signature()
{
  return &sig_;
}

void rpc::upd_test::request( json_wtr& msg )
{
  // construct binary transaction
  net_buf *bptr = net_buf::alloc();
  bincode tx( bptr->buf_ );

  // signatures section
  tx.add_len<2>();      // two signatures (funding and account)
  size_t pub_idx = tx.reserve_sign();
  size_t tst_idx = tx.reserve_sign();

  // message header
  size_t tx_idx = tx.get_pos();
  tx.add( (uint8_t)2 ); // funding, test are signing accounts
  tx.add( (uint8_t)0 ); // read-only signed accounts
  tx.add( (uint8_t)1 ); // program are read-only unsigned accounts

  // accounts
  tx.add_len<3>();      // 3 accounts: publish, test, and program
  tx.add( *pkey_ );     // publish account
  tx.add( *tkey_ );     // test account
  tx.add( *gkey_ );     // programid

  // recent block hash
  tx.add( *bhash_ );     // recent block hash

  // instructions section
  tx.add_len<1>();      // one instruction
  tx.add( (uint8_t)2);  // program_id index
  tx.add_len<2>();      // 2 accounts: publish, test
  tx.add( (uint8_t)0 ); // index of publish account
  tx.add( (uint8_t)1 ); // index of test account

  // instruction parameter section
  tx.add_len<sizeof(cmd_upd_test)>();     // size of data array
  tx.add( (const char*)upd_, sizeof( upd_ ) );

  // both publish and param sign message
  tx.sign( pub_idx, tx_idx, *pkey_ );
  sig_.init_from_buf( (const uint8_t*)(tx.get_buf() + pub_idx) );
  tx.sign( tst_idx, tx_idx, *tkey_ );

  // encode transaction and add to json params
  msg.add_key( "method", "sendTransaction" );
  msg.add_key( "params", json_wtr::e_arr );
  char buf[4096];
  size_t buf_len = enc_base64( (const uint8_t*)tx.get_buf(),
    tx.size(), buf );
  msg.add_val( str( buf, buf_len ) );
  msg.add_val( json_wtr::e_obj );
  msg.add_key( "encoding", "base64" );
  msg.pop();
  msg.pop();
  bptr->dealloc();
}

void rpc::upd_test::response( const jtree& jt )
{
  if ( on_error( jt, this ) ) return;
  on_response( this );
}

