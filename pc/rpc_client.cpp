#include "rpc_client.hpp"
#include "bincode.hpp"
#include <program/src/oracle/oracle.h>
#include <iostream>

using namespace pc;

// system program instructions
enum system_instruction : uint32_t
{
  e_create_account,
  e_assign,
  e_transfer
};

static hash gen_sys_id()
{
  pc::hash id;
  id.zero();
  return id;
}

// system program id
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
// rpc_client

rpc_client::rpc_client()
: hptr_( nullptr ),
  wptr_( nullptr ),
  id_( 0UL )
{
  hp_.cp_ = this;
  wp_.cp_ = this;
}

void rpc_client::set_http_conn( net_connect *hptr )
{
  hptr_ = hptr;
  hptr_->set_net_parser( &hp_ );
}

net_connect *rpc_client::get_http_conn() const
{
  return hptr_;
}

void rpc_client::set_ws_conn( net_connect *wptr )
{
  wptr_ = wptr;
  wptr_->set_net_parser( &wp_ );
  wp_.set_net_connect( wptr_ );
}

net_connect *rpc_client::get_ws_conn() const
{
  return wptr_;
}

void rpc_client::send( rpc_request *rptr )
{
  // get request id
  uint64_t id;
  if ( !reuse_.empty() ) {
    id = reuse_.back();
    reuse_.pop_back();
  } else {
    id = ++id_;
    rv_.resize( 1 + id, nullptr );
  }
  rptr->set_id( id );
  rptr->set_rpc_client( this );
  rptr->set_sent_time( get_now() );
  rv_[id] = rptr;

  // construct json message
  json_wtr jw;
  jw.add_val( json_wtr::e_obj );
  jw.add_key( "jsonrpc", "2.0" );
  jw.add_key( "id", id );
  rptr->request( jw );
  jw.pop();
  jw.print();
  if ( rptr->get_is_http() ) {
    // submit http POST request
    http_request msg;
    msg.init( "POST", "/" );
    msg.add_hdr( "Content-Type", "application/json" );
    msg.commit( jw );
    hptr_->add_send( msg );
  } else {
    // submit websocket message
    ws_wtr msg;
    msg.commit( ws_wtr::text_id, jw, true );
    wptr_->add_send( msg );
  }
}

void rpc_client::rpc_http::parse_content( const char *txt, size_t len )
{
  cp_->parse_response( txt, len );
}

void rpc_client::rpc_ws::parse_msg( const char *txt, size_t len )
{
  cp_->parse_response( txt, len );
}

void rpc_client::parse_response( const char *txt, size_t len )
{
  std::cout.write( txt, len );
  std::cout << std::endl;

  // parse and redirect response to corresponding request
  jp_.parse( txt, len );
  uint32_t idtok = jp_.find_val( 1, "id" );
  if ( idtok ) {
    // response to http request
    uint64_t id = jp_.get_uint( idtok );
    if ( id < rv_.size() ) {
      rpc_request *rptr = rv_[id];
      if ( rptr ) {
        rv_[id] = nullptr;
        reuse_.push_back( id );
        rptr->response( jp_ );
      }
    }
  } else {
    // websocket notification
    uint32_t ptok = jp_.find_val( 1, "params" );
    uint32_t stok = jp_.find_val( ptok, "subscription" );
    if ( stok ) {
      uint64_t id = jp_.get_uint( stok );
      sub_map_t::iterator i = smap_.find( id );
      if ( i != smap_.end() && (*i).second->notify( jp_ ) ) {
        smap_.erase( i );
      }
    }
  }
}

void rpc_client::add_notify( rpc_request *rptr )
{
  smap_[rptr->get_id()] = rptr;
}

void rpc_client::remove_notify( rpc_request *rptr )
{
  sub_map_t::iterator i = smap_.find( rptr->get_id() );
  if ( i != smap_.end() ) {
    smap_.erase( i );
  }
}

///////////////////////////////////////////////////////////////////////////
// rpc_request

rpc_sub::~rpc_sub()
{
}

rpc_request::rpc_request()
: cb_( nullptr ),
  cp_( nullptr ),
  id_( 0UL ),
  ec_( 0 ),
  sent_ts_( 0L ),
  recv_ts_( 0L )
{
}

rpc_request::~rpc_request()
{
}

void rpc_request::set_sub( rpc_sub *cb )
{
  cb_ = cb;
}

rpc_sub *rpc_request::get_sub() const
{
  return cb_;
}

void rpc_request::set_rpc_client( rpc_client *cptr )
{
  cp_ = cptr;
}

rpc_client *rpc_request::get_rpc_client() const
{
  return cp_;
}

void rpc_request::set_id( uint64_t id )
{
  id_ = id;
}

uint64_t rpc_request::get_id() const
{
  return id_;
}

void rpc_request::set_err_code( int ecode )
{
  ec_ = ecode;
}

int rpc_request::get_err_code() const
{
  return ec_;
}

void rpc_request::set_sent_time( int64_t sent_ts )
{
  sent_ts_ = sent_ts;
}

int64_t rpc_request::get_sent_time() const
{
  return sent_ts_;
}

void rpc_request::set_recv_time( int64_t recv_ts )
{
  recv_ts_ = recv_ts;
}

int64_t rpc_request::get_recv_time() const
{
  return recv_ts_;
}

bool rpc_request::get_is_recv() const
{
  return recv_ts_ >= sent_ts_;
}

bool rpc_request::get_is_http() const
{
  return true;
}

bool rpc_request::notify( const jtree& )
{
  return true;
}

bool rpc_subscription::get_is_http() const
{
  return false;
}

void rpc_subscription::add_notify( const jtree& jp )
{
  uint32_t rtok  = jp.find_val( 1, "result" );
  if ( rtok ) {
    uint64_t subid = jp.get_uint( rtok );
    set_id( subid );
    get_rpc_client()->add_notify( this );
  }
}

void rpc_subscription::remove_notify()
{
  get_rpc_client()->remove_notify( this );
}

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
// get_account_info

void rpc::get_account_info::set_account( pub_key *acc )
{
  acc_ = acc;
}

void rpc::get_account_info::set_commitment( commitment val )
{
  cmt_ = val;
}

uint64_t rpc::get_account_info::get_slot() const
{
  return slot_;
}

bool rpc::get_account_info::get_is_executable() const
{
  return is_exec_;
}

uint64_t rpc::get_account_info::get_lamports() const
{
  return lamports_;
}

uint64_t rpc::get_account_info::get_rent_epoch() const
{
  return rent_epoch_;
}

void rpc::get_account_info::get_owner( const char *&optr, size_t& olen) const
{
  optr = optr_;
  olen = olen_;
}

rpc::get_account_info::get_account_info()
: slot_(0),
  lamports_( 0 ),
  rent_epoch_( 0 ),
  dptr_( nullptr ),
  dlen_( 0 ),
  optr_( nullptr ),
  olen_( 0 ),
  is_exec_( false ),
  cmt_( commitment::e_finalized )
{
}

void rpc::get_account_info::request( json_wtr& msg )
{
  msg.add_key( "method", "getAccountInfo" );
  msg.add_key( "params", json_wtr::e_arr );
  msg.add_val( *acc_ );
  msg.add_val( json_wtr::e_obj );
  msg.add_key( "encoding", "base64" );
  msg.add_key( "commitment", commitment_to_str( cmt_ ) );
  msg.pop();
  msg.pop();
}

void rpc::get_account_info::response( const jtree& jt )
{
  if ( on_error( jt, this ) ) return;
  uint32_t rtok = jt.find_val( 1, "result" );
  uint32_t ctok = jt.find_val( rtok, "context" );
  slot_ = jt.get_uint( jt.find_val( ctok, "slot" ) );
  uint32_t vtok = jt.find_val( rtok, "value" );
  is_exec_ = jt.get_bool( jt.find_val( vtok, "executable" ) );
  lamports_ = jt.get_uint( jt.find_val( vtok, "lamports" ) );
  uint32_t dtok = jt.find_val( vtok, "data" );
  jt.get_text( jt.get_first( dtok ), dptr_, dlen_ );
  jt.get_text( jt.find_val( vtok, "owner" ), optr_, olen_ );
  rent_epoch_ = jt.get_uint( jt.find_val( vtok, "rentEpoch" ) );
  on_response( this );
}

///////////////////////////////////////////////////////////////////////////
// get_recent_block_hash

uint64_t rpc::get_recent_block_hash::get_slot() const
{
  return slot_;
}

hash *rpc::get_recent_block_hash::get_block_hash()
{
  return &bhash_;
}

uint64_t rpc::get_recent_block_hash::get_lamports_per_signature() const
{
  return fee_per_sig_;
}

rpc::get_recent_block_hash::get_recent_block_hash()
: slot_( 0 ),
  fee_per_sig_( 0 )
{
  bhash_.zero();
}

void rpc::get_recent_block_hash::request( json_wtr& msg )
{
  msg.add_key( "method", "getRecentBlockhash" );
}

void rpc::get_recent_block_hash::response( const jtree& jt )
{
  if ( on_error( jt, this ) ) return;
  uint32_t rtok = jt.find_val( 1, "result" );
  uint32_t ctok = jt.find_val( rtok, "context" );
  slot_ = jt.get_uint( jt.find_val( ctok, "slot" ) );
  uint32_t vtok = jt.find_val( rtok, "value" );
  size_t txt_len = 0;
  const char *txt;
  jt.get_text( jt.find_val( vtok, "blockhash" ), txt, txt_len );
  bhash_.dec_base58( (const uint8_t*)txt, txt_len );
  uint32_t ftok = jt.find_val( vtok, "feeCalculator" );
  fee_per_sig_ = jt.get_uint( jt.find_val( ftok, "lamportsPerSignature" ) );
  on_response( this );
}

///////////////////////////////////////////////////////////////////////////
// get_health

void rpc::get_health::request( json_wtr& msg )
{
  msg.add_key( "method", "getHealth" );
}

void rpc::get_health::response( const jtree& jt )
{
  if ( on_error( jt, this ) ) return;
  on_response( this );
}

///////////////////////////////////////////////////////////////////////////
// transfer

void rpc::transfer::set_block_hash( hash *bhash )
{
  bhash_ = bhash;
}

void rpc::transfer::set_sender( key_pair *snd )
{
  snd_ = snd;
}

void rpc::transfer::set_receiver( pub_key *rcv )
{
  rcv_ = rcv;
}

void rpc::transfer::set_lamports( uint64_t funds )
{
  lamports_ = funds;
}

signature *rpc::transfer::get_signature()
{
  return &sig_;
}

void rpc::transfer::enc_signature( std::string& sig )
{
  sig_.enc_base58( sig );
}


rpc::transfer::transfer()
: lamports_( 0UL )
{
}

void rpc::transfer::request( json_wtr& msg )
{
  // construct binary transaction
  net_buf *bptr = net_buf::alloc();
  bincode tx( bptr->buf_ );

  // signatures section
  tx.add_len<1>();      // one signature
  size_t sign_idx = tx.reserve_sign();

  // message header
  size_t tx_idx = tx.get_pos();
  tx.add( (uint8_t)1 ); // signing accounts
  tx.add( (uint8_t)0 ); // read-only signed accounts
  tx.add( (uint8_t)1 ); // read-only unsigned accounts

  // accounts
  tx.add_len<3>();      // 3 accounts: sender, receiver, program
  tx.add( *snd_ );      // sender account
  tx.add( *rcv_ );      // receiver account
  tx.add( sys_id );     // system programid

  // recent block hash
  tx.add( *bhash_ );    // recent block hash

  // instructions section
  tx.add_len<1>();      // one instruction
  tx.add( (uint8_t)2);  // program_id index
  tx.add_len<2>();      // 2 accounts: sender, receiver
  tx.add( (uint8_t)0 ); // index of sender account
  tx.add( (uint8_t)1 ); // index of receiver account

  // instruction parameter section
  tx.add_len<12>();     // size of data array
  tx.add( (uint32_t)system_instruction::e_transfer );
  tx.add( (uint64_t)lamports_ );

  // sign message
  tx.sign( sign_idx, tx_idx, *snd_ );
  sig_.init_from_buf( (const uint8_t*)(tx.get_buf() + sign_idx) );

  // encode transaction and add to json params
  msg.add_key( "method", "sendTransaction" );
  msg.add_key( "params", json_wtr::e_arr );
  msg.add_val_enc_base64( str( tx.get_buf(), tx.size() ) );
  msg.add_val( json_wtr::e_obj );
  msg.add_key( "encoding", "base64" );
  msg.pop();
  msg.pop();
  bptr->dealloc();
}

void rpc::transfer::response( const jtree& jt )
{
  if ( on_error( jt, this ) ) return;
  on_response( this );
}

///////////////////////////////////////////////////////////////////////////
// account_subscribe

rpc::account_subscribe::account_subscribe()
: slot_( 0L ),
  lamports_( 0L ),
  dlen_( 0 ),
  dptr_( nullptr ),
  cmt_( commitment::e_finalized )
{
}

void rpc::account_subscribe::set_account( pub_key *pkey )
{
  acc_ = pkey;
}

void rpc::account_subscribe::set_commitment( commitment val )
{
  cmt_ = val;
}

uint64_t rpc::account_subscribe::get_slot() const
{
  return slot_;
}

uint64_t rpc::account_subscribe::get_lamports() const
{
  return lamports_;
}

void rpc::account_subscribe::request( json_wtr& msg )
{
  msg.add_key( "method", "accountSubscribe" );
  msg.add_key( "params", json_wtr::e_arr );
  msg.add_val( *acc_ );
  msg.add_val( json_wtr::e_obj );
  msg.add_key( "encoding", "base64" );
  msg.add_key( "commitment", commitment_to_str( cmt_ ) );
  msg.pop();
  msg.pop();
}

void rpc::account_subscribe::response( const jtree& jt )
{
  if ( on_error( jt, this ) ) return;

  // add to notification list
  add_notify( jt );
}

bool rpc::account_subscribe::notify( const jtree& jt )
{
  if ( on_error( jt, this ) ) return true;

  uint32_t ptok = jt.find_val( 1, "params" );
  uint32_t rtok = jt.find_val( ptok, "result" );
  uint32_t ctok = jt.find_val( rtok, "context" );
  slot_ = jt.get_uint( jt.find_val( ctok, "slot" ) );
  uint32_t vtok = jt.find_val( rtok, "value" );
  uint32_t dtok = jt.find_val( vtok, "data" );
  jt.get_text( jt.get_first( dtok ), dptr_, dlen_ );
  lamports_ = jt.get_uint( jt.find_val( vtok, "lamports" ) );

  on_response( this );
  return false;  // keep notification
}

///////////////////////////////////////////////////////////////////////////
// signature_subscribe

rpc::signature_subscribe::signature_subscribe()
: cmt_( commitment::e_finalized )
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

  on_response( this );
  return true;  // remove notification
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
// add_symbol

void rpc::add_symbol::set_symbol( const symbol& sym )
{
  sym_ = sym;
}

symbol *rpc::add_symbol::get_symbol()
{
  return &sym_;
}

void rpc::add_symbol::set_exponent( int32_t expo )
{
  expo_ = expo;
}

void rpc::add_symbol::set_program( pub_key *gkey )
{
  gkey_ = gkey;
}

void rpc::add_symbol::set_block_hash( hash *bhash )
{
  bhash_ = bhash;
}

void rpc::add_symbol::set_publish( key_pair *pkey )
{
  pkey_ = pkey;
}

void rpc::add_symbol::set_account( key_pair *akey )
{
  akey_ = akey;
}

void rpc::add_symbol::set_mapping( key_pair *mkey )
{
  mkey_ = mkey;
}

signature *rpc::add_symbol::get_signature()
{
  return &sig_;
}

rpc::add_symbol::add_symbol()
: num_( 0 ), expo_( 0 )
{
}

void rpc::add_symbol::request( json_wtr& msg )
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
  tx.add_len<4>();      // 4 accounts: publish, symbol, mapping*, program
  tx.add( *pkey_ );     // publish account
  tx.add( *mkey_ );     // mapping account
  tx.add( *akey_ );     // symbol account
  tx.add( *gkey_ );     // programid

  // recent block hash
  tx.add( *bhash_ );    // recent block hash

  // instructions section
  tx.add_len<1>();      // one instruction
  tx.add( (uint8_t)3);  // program_id index
  tx.add_len<3>();      // 3 accounts: publish, symbol, mapping
  tx.add( (uint8_t)0 ); // index of publish account
  tx.add( (uint8_t)1 ); // index of mapping account
  tx.add( (uint8_t)2 ); // index of symbol account

  // instruction parameter section
  tx.add_len<sizeof(cmd_add_symbol)>();
  tx.add( (int32_t)e_cmd_add_symbol );
  tx.add( (int32_t)expo_ );
  tx.add( sym_ );

  // all accounts need to sign transaction
  tx.sign( pub_idx, tx_idx, *pkey_ );
  sig_.init_from_buf( (const uint8_t*)(tx.get_buf() + pub_idx) );
  tx.sign( map_idx, tx_idx, *mkey_ );
  tx.sign( sym_idx, tx_idx, *akey_ );

  // encode transaction and add to json params
  send_transaction( msg, tx );
  bptr->dealloc();
}

void rpc::add_symbol::response( const jtree& jt )
{
  if ( on_error( jt, this ) ) return;
  on_response( this );
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
  tx.add_len<4>();     // size of data array
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
// add_publisher

void rpc::add_publisher::set_symbol( const symbol& sym )
{
  sym_= sym;
}

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

symbol *rpc::add_publisher::get_symbol()
{
  return &sym_;
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
  tx.add_len<2>();      // two signatures (publish, symbol )
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
  tx.add( (int32_t)e_cmd_add_publisher );
  tx.add( (int32_t)0 );
  tx.add( sym_ );
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
// upd_price

void rpc::upd_price::set_symbol( symbol *sym )
{
  sym_ = sym;
}

void rpc::upd_price::set_publish( key_pair *pk )
{
  pkey_ = pk;
}

void rpc::upd_price::set_account( pub_key *akey )
{
  akey_ = akey;
}

void rpc::upd_price::set_program( pub_key *gkey )
{
  gkey_ = gkey;
}

void rpc::upd_price::set_block_hash( hash *bhash )
{
  bhash_ = bhash;
}

signature *rpc::upd_price::get_signature()
{
  return &sig_;
}

void rpc::upd_price::set_price( int64_t px, uint64_t conf )
{
  price_ = px;
  conf_ = conf;
}

void rpc::upd_price::request( json_wtr& msg )
{
  // construct binary transaction
  net_buf *bptr = net_buf::alloc();
  bincode tx( bptr->buf_ );

  // signatures section
  tx.add_len<1>();      // one signature (publish)
  size_t pub_idx = tx.reserve_sign();

  // message header
  size_t tx_idx = tx.get_pos();
  tx.add( (uint8_t)1 ); // pub is only signing account
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
  tx.add_len<sizeof(cmd_upd_price)>();
  tx.add( (int32_t)e_cmd_upd_price );
  tx.add( (int32_t)0 );
  tx.add( *sym_ );
  tx.add( price_ );
  tx.add( conf_ );

  // all accounts need to sign transaction
  tx.sign( pub_idx, tx_idx, *pkey_ );
  sig_.init_from_buf( (const uint8_t*)(tx.get_buf() + pub_idx) );

  // encode transaction and add to json params
  send_transaction( msg, tx );
  bptr->dealloc();
}

void rpc::upd_price::response( const jtree& jt )
{
  if ( on_error( jt, this ) ) return;
  on_response( this );
}
