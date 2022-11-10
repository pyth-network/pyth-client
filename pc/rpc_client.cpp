#include "rpc_client.hpp"
#include "bincode.hpp"
#include <unistd.h>
#include "log.hpp"
#include <zstd.h>

using namespace pc;

// price_types
const char *price_type_str[] = {
  "unknown",
  "price",
  "twap"
};

// symbol_status
const char *symbol_status_str[] = {
  "unknown",
  "trading",
  "halted"
};

// block(slot) commitment
static const char *commitment_str[] = {
  "unknown",
  "processed",
  "confirmed",
  "finalized"
};

namespace pc
{
  price_type str_to_price_type( str s )
  {
    for( unsigned i=0; i != (unsigned)price_type::e_last_price_type; ++i ) {
      if ( s == price_type_str[i] ) {
        return (price_type)i;
      }
    }
    return price_type::e_unknown;
  }

  str price_type_to_str( price_type ptype )
  {
    unsigned iptype = (unsigned)ptype;
    return price_type_str[iptype<(unsigned)price_type::e_last_price_type?
      iptype: 0 ];
  }

  symbol_status str_to_symbol_status( str s )
  {
    for( unsigned i=0;
         i != (unsigned)symbol_status::e_last_symbol_status; ++i ) {
      if ( s == symbol_status_str[i] ) {
        return (symbol_status)i;
      }
    }
    return symbol_status::e_unknown;
  }

  str symbol_status_to_str( symbol_status st )
  {
    unsigned ist = (unsigned)st;
    return symbol_status_str[
      ist<(unsigned)symbol_status::e_last_symbol_status? ist: 0 ];
  }

  str commitment_to_str( commitment val )
  {
    unsigned iv = (unsigned)val;
    if ( iv >= (unsigned)commitment::e_last_commitment ) {
      iv = 0;
    }
    return commitment_str[iv];
  }

  commitment str_to_commitment( str s )
  {
    for( unsigned i=0;
         i != (unsigned)commitment::e_last_commitment; ++i ) {
      if ( s == commitment_str[i] ) {
        return (commitment)i;
      }
    }
    return commitment::e_unknown;
  }

}

///////////////////////////////////////////////////////////////////////////
// rpc_client

rpc_client::rpc_client()
: hptr_( nullptr ),
  wptr_( nullptr ),
  id_( 0UL ),
  cxt_( nullptr )
{
  hp_.cp_ = this;
  wp_.cp_ = this;
  cxt_ = ZSTD_createDCtx();
}

rpc_client::~rpc_client()
{
  if ( cxt_ ) {
    ZSTD_freeDCtx( (ZSTD_DCtx*)cxt_ );
    cxt_ = nullptr;
  }
}

void rpc_client::set_http_conn( tcp_connect *hptr )
{
  hptr_ = hptr;
  hptr_->set_net_parser( &hp_ );
}

tcp_connect *rpc_client::get_http_conn() const
{
  return hptr_;
}

void rpc_client::set_ws_conn( net_connect *wptr )
{
  wptr_ = wptr;
  if ( wptr_ ) {
    wptr_->set_net_parser( &wp_ );
  }
  wp_.set_net_connect( wptr_ );
}

net_connect *rpc_client::get_ws_conn() const
{
  return wptr_;
}

void rpc_client::reset()
{
  rv_.clear();
  smap_.clear();
  reuse_.clear();
  id_ = 0;
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
  }
  rptr->set_id( id );
  rptr->set_rpc_client( this );
  rptr->set_sent_time( get_now() );
  rv_.emplace( id, rptr );

  // construct json message
  json_wtr jw;
  jw.add_val( json_wtr::e_obj );
  jw.add_key( "jsonrpc", "2.0" );
  jw.add_key( "id", id );
  rptr->request( jw );
  jw.pop();
//  jw.print();
  if ( rptr->get_is_http() ) {
    // submit http POST request
    http_request msg;
    msg.init( "POST", "/" );
    msg.add_hdr( "Host", hptr_->get_host() );
    msg.add_hdr( "Content-Type", "application/json" );
    msg.commit( jw );
    hptr_->add_send( msg );
  } else if ( wptr_ ) {
    // submit websocket message
    ws_wtr msg;
    msg.commit( ws_wtr::text_id, jw, true );
    wptr_->add_send( msg );
  }
  else {
    PC_LOG_WRN( "no ws connection to send msg" ).end();
  }
}

void rpc_client::send( rpc::upd_price *upds[], const unsigned n, unsigned cu_units, unsigned cu_price )
{
  if ( ! n ) {
    return;
  }

  // get request id
  uint64_t id;
  if ( !reuse_.empty() ) {
    id = reuse_.back();
    reuse_.pop_back();
  } else {
    id = ++id_;
  }
  const auto now = get_now();
  for ( unsigned i = 0; i < n; ++i ) {
    rpc_request *const rptr = upds[ i ];
    rptr->set_id( id );
    rptr->set_rpc_client( this );
    rptr->set_sent_time( now );
    rv_.emplace( id, rptr );
  }

  // construct json message
  json_wtr jw;
  jw.add_val( json_wtr::e_obj );
  jw.add_key( "jsonrpc", "2.0" );
  jw.add_key( "id", id );
  rpc::upd_price::request( jw, upds, n, cu_units, cu_price );
  jw.pop();
//  jw.print();
  if ( upds[ 0 ]->get_is_http() ) {
    // submit http POST request
    http_request msg;
    msg.init( "POST", "/" );
    msg.add_hdr( "Host", hptr_->get_host() );
    msg.add_hdr( "Content-Type", "application/json" );
    msg.commit( jw );
    hptr_->add_send( msg );
  } else if ( wptr_ ) {
    // submit websocket message
    ws_wtr msg;
    msg.commit( ws_wtr::text_id, jw, true );
    wptr_->add_send( msg );
  }
  else {
    PC_LOG_WRN( "no ws connection to send msg" ).end();
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
  // parse and redirect response to corresponding request
  jp_.parse( txt, len );
  uint32_t idtok = jp_.find_val( 1, "id" );
  if ( idtok ) {
    // response to http request
    const uint64_t id = jp_.get_uint( idtok );
    const auto range = rv_.equal_range( id );
    for ( auto it = range.first; it != range.second; ++it ) {
      rpc_request *const rptr = it->second;
      rptr->response( jp_ );
    }
    reuse_.push_back( id );
    rv_.erase( range.first, range.second );
  } else {
    // websocket notification
    uint32_t ptok = jp_.find_val( 1, "params" );
    uint32_t stok = jp_.find_val( ptok, "subscription" );
    if ( stok ) {
      uint64_t id = jp_.get_uint( stok );
      sub_map_t::iter_t i = smap_.find( id );
      if ( i  && smap_.obj(i)->notify( jp_ ) ) {
        smap_.del( i );
      }
    }
  }
}

void rpc_client::add_notify( rpc_request *rptr )
{
  smap_.ref( smap_.add( rptr->get_id() ) ) = rptr;
}

void rpc_client::remove_notify( rpc_request *rptr )
{
  sub_map_t::iter_t i = smap_.find( rptr->get_id() );
  if ( i ) {
    smap_.del( i );
  }
}

size_t rpc_client::get_data_ref(
    const char *dptr, size_t dlen, size_t tlen, char *&ptr )
{
  tlen = ZSTD_compressBound( tlen );
  abuf_.resize( dlen );
  zbuf_.resize( tlen );
  ZSTD_DCtx *cxt = (ZSTD_DCtx*)cxt_;
  dlen = dec_base64( dptr, dlen, (uint8_t*)&abuf_[0] );
  tlen = ZSTD_decompressDCtx( cxt, &zbuf_[0], tlen, &abuf_[0], dlen );
  ptr = &zbuf_[0];
  return tlen;
}

size_t rpc_client::get_data_val(
    const char *dptr, size_t dlen, size_t tlen, char *tgt )
{
  abuf_.resize( dlen );
  ZSTD_DCtx *cxt = (ZSTD_DCtx*)cxt_;
  dlen = dec_base64( dptr, dlen, (uint8_t*)&abuf_[0] );
  tlen = ZSTD_decompressDCtx( cxt, tgt, tlen, &abuf_[0], dlen );
  return tlen;
}

///////////////////////////////////////////////////////////////////////////
// rpc_request

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
// get_slot

rpc::get_slot::get_slot( commitment const cmt )
: cmt_{ cmt }
, cslot_( 0UL )
{
}

uint64_t rpc::get_slot::get_current_slot() const
{
  return cslot_;
}

void rpc::get_slot::request( json_wtr& msg )
{
  msg.add_key( "method", "getSlot" );
  msg.add_key( "params", json_wtr::e_arr );
  msg.add_val( json_wtr::e_obj );
  msg.add_key( "commitment", commitment_to_str( cmt_ ) );
  msg.pop();
  msg.pop();
}

void rpc::get_slot::response( const jtree& jt )
{
  if ( on_error( jt, this ) ) return;
  uint32_t rtok = jt.find_val( 1, "result" );
  cslot_ = jt.get_uint( rtok );
  on_response( this );
}

///////////////////////////////////////////////////////////////////////////
// account_update

rpc::account_update::account_update()
: acc_{},
  cmt_{ commitment::e_confirmed },
  slot_{ 0UL },
  lamports_{ 0UL },
  dlen_{ 0UL },
  dptr_{ nullptr }
{
}

pub_key const* rpc::account_update::get_account() const
{
  return &acc_;
}

void rpc::account_update::set_account( pub_key *pkey )
{
  acc_ = *pkey;
}

void rpc::account_update::set_commitment( commitment val )
{
  cmt_ = val;
}

uint64_t rpc::account_update::get_slot() const
{
  return slot_;
}

uint64_t rpc::account_update::get_lamports() const
{
  return lamports_;
}

///////////////////////////////////////////////////////////////////////////
// get_account_info

bool rpc::get_account_info::get_is_executable() const
{
  return is_exec_;
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
: account_update{},
  rent_epoch_( 0 ),
  optr_( nullptr ),
  olen_( 0 ),
  is_exec_( false )
{
}

void rpc::get_account_info::request( json_wtr& msg )
{
  msg.add_key( "method", "getAccountInfo" );
  msg.add_key( "params", json_wtr::e_arr );
  msg.add_val( acc_ );
  msg.add_val( json_wtr::e_obj );
  msg.add_key( "encoding", "base64+zstd" );
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

bool rpc::get_account_info::get_is_http() const
{
  return true;
}

///////////////////////////////////////////////////////////////////////////
// account_subscribe

rpc::account_subscribe::account_subscribe()
: account_update{}
{
}

void rpc::account_subscribe::request( json_wtr& msg )
{
  msg.add_key( "method", "accountSubscribe" );
  msg.add_key( "params", json_wtr::e_arr );
  msg.add_val( acc_ );
  msg.add_val( json_wtr::e_obj );
  msg.add_key( "encoding", "base64+zstd" );
  msg.add_key( "commitment", commitment_to_str( cmt_ ) );
  msg.pop();
  msg.pop();
}

void rpc::account_subscribe::response( const jtree& jt )
{
  auto* const this_t = static_cast< account_update* >( this );

  if ( on_error( jt, this_t ) ) return;

  // add to notification list
  add_notify( jt );
}

bool rpc::account_subscribe::notify( const jtree& jt )
{
  auto* const this_t = static_cast< account_update* >( this );

  if ( on_error( jt, this_t ) ) return true;

  uint32_t ptok = jt.find_val( 1, "params" );
  uint32_t rtok = jt.find_val( ptok, "result" );
  uint32_t ctok = jt.find_val( rtok, "context" );
  slot_ = jt.get_uint( jt.find_val( ctok, "slot" ) );
  uint32_t vtok = jt.find_val( rtok, "value" );
  uint32_t dtok = jt.find_val( vtok, "data" );
  jt.get_text( jt.get_first( dtok ), dptr_, dlen_ );
  lamports_ = jt.get_uint( jt.find_val( vtok, "lamports" ) );

  on_response( this_t );
  return false;  // keep notification
}

///////////////////////////////////////////////////////////////////////////
// program_subscribe

rpc::program_subscribe::program_subscribe()
: account_update{}
, pgm_{ nullptr }
{
}

void rpc::program_subscribe::set_program( pub_key *pkey )
{
  pgm_ = pkey;
}

void rpc::program_subscribe::request( json_wtr& msg )
{
  msg.add_key( "method", "programSubscribe" );
  msg.add_key( "params", json_wtr::e_arr );
  msg.add_val( *pgm_ );
  msg.add_val( json_wtr::e_obj );
  msg.add_key( "encoding", "base64+zstd" );
  msg.add_key( "commitment", commitment_to_str( cmt_ ) );
  msg.pop();
  msg.pop();
}

void rpc::program_subscribe::response( const jtree& jt )
{
  auto* const this_t = static_cast< account_update* >( this );

  if ( on_error( jt, this_t ) ) return;

  // add to notification list
  add_notify( jt );
}

bool rpc::program_subscribe::notify( const jtree& jt )
{
  auto* const this_t = static_cast< account_update* >( this );

  if ( on_error( jt, this_t ) ) return true;

  uint32_t ptok = jt.find_val( 1, "params" );
  uint32_t rtok = jt.find_val( ptok, "result" );
  uint32_t ctok = jt.find_val( rtok, "context" );
  slot_ = jt.get_uint( jt.find_val( ctok, "slot" ) );
  uint32_t vtok = jt.find_val( rtok, "value" );
  str akey = jt.get_str( jt.find_val( vtok, "pubkey" ) );
  acc_.init_from_text( akey );
  uint32_t atok = jt.find_val( vtok, "account" );
  uint32_t dtok = jt.find_val( atok, "data" );
  jt.get_text( jt.get_first( dtok ), dptr_, dlen_ );
  lamports_ = jt.get_uint( jt.find_val( atok, "lamports" ) );

  on_response( this_t );
  return false;  // keep notification
}

///////////////////////////////////////////////////////////////////////////
// get_program_accounts

rpc::get_program_accounts::get_program_accounts()
: account_update{}
, pgm_{ nullptr }
, acct_type_{ 0 }
{
}

void rpc::get_program_accounts::set_program( pub_key *pkey )
{
  pgm_ = pkey;
}

void rpc::get_program_accounts::set_account_type( uint32_t const acct_type )
{
  acct_type_ = acct_type;
}

void rpc::get_program_accounts::request( json_wtr& msg )
{
  msg.add_key( "method", "getProgramAccounts" );
  msg.add_key( "params", json_wtr::e_arr );
  msg.add_val( *pgm_ );
  msg.add_val( json_wtr::e_obj );
  msg.add_key( "encoding", "base64+zstd" );
  msg.add_key( "commitment", commitment_to_str( cmt_ ) );
  if ( acct_type_ ) {
    msg.add_key( "filters", json_wtr::e_arr );
    msg.add_val( json_wtr::e_obj );
    msg.add_key( "memcmp", json_wtr::e_obj );
    msg.add_key( "offset", offsetof( pc_acc_t, type_ ) );
    msg.add_key_enc_base58(
      "bytes", str( ( char* )&acct_type_, sizeof( acct_type_ ) ) );
    msg.pop();
    msg.pop();
    msg.pop();
  }
  msg.add_key( "withContext", json_wtr::jtrue{} );
  msg.pop();
  msg.pop();
}

void rpc::get_program_accounts::response( const jtree& jt )
{
  auto* const this_t = static_cast< account_update* >( this );

  if ( on_error( jt, this_t ) ) return;
  uint32_t const rtok = jt.find_val( 1, "result" );
  uint32_t const ctok = jt.find_val( rtok, "context" );
  slot_ = jt.get_uint( jt.find_val( ctok, "slot" ) );
  uint32_t const vtok = jt.find_val( rtok, "value" );
  for ( uint32_t tok = jt.get_first( vtok ); tok; tok = jt.get_next( tok ) ) {
    str akey = jt.get_str( jt.find_val( tok, "pubkey" ) );
    acc_.init_from_text( akey );
    uint32_t const atok = jt.find_val( tok, "account" );
    lamports_ = jt.get_uint( jt.find_val( atok, "lamports" ) );
    uint32_t dtok = jt.find_val( atok, "data" );
    jt.get_text( jt.get_first( dtok ), dptr_, dlen_ );

    on_response( this_t );
  }
}

bool rpc::get_program_accounts::get_is_http() const
{
  return true;
}

///////////////////////////////////////////////////////////////////////////
// upd_price

rpc::upd_price::upd_price()
: pkey_( nullptr ),
  ckey_( nullptr ),
  gkey_( nullptr ),
  akey_( nullptr ),
  cmd_( e_cmd_upd_price_no_fail_on_error )
{
}

void rpc::upd_price::set_symbol_status( symbol_status st )
{
  st_ = st;
}

void rpc::upd_price::set_publish( key_pair *pk )
{
  pkey_ = pk;
}

void rpc::upd_price::set_pubcache( key_cache *pk )
{
  ckey_ = pk;
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

void rpc::upd_price::set_price( int64_t px,
                                uint64_t conf,
                                symbol_status st,
                                bool is_agg )
{
  price_ = px;
  conf_  = conf;
  st_    = st;
  cmd_   = is_agg?e_cmd_agg_price:e_cmd_upd_price_no_fail_on_error;
}

void rpc::upd_price::set_slot( const uint64_t pub_slot )
{
  pub_slot_ = pub_slot;
}

uint64_t rpc::upd_price::get_slot() {
  return pub_slot_;
}

signature *rpc::upd_price::get_signature()
{
  return &sig_;
}

str rpc::upd_price::get_ack_signature() const
{
  return ack_sig_;
}

class tx_wtr : public net_wtr
{
public:
  void init( bincode& tx ) {
    tx.attach( hd_->buf_ );
    tx.add( (uint16_t)PC_TPU_PROTO_ID );
    tx.add( (uint16_t)0 );
  }
  void commit( bincode& tx ) {
    tx_hdr *hdr = (tx_hdr*)hd_->buf_;
    hd_->size_ = tx.size();
    hdr->size_ = tx.size();
  }
};

bool rpc::upd_price::build_tx(
  bincode& tx, upd_price* upds[], const unsigned n, unsigned cu_units, unsigned cu_price
)
{
  if ( ! n ) {
    return false;
  }

  // signatures section
  tx.add_len< 1 >(); // one signature (publish)
  size_t pub_idx = tx.reserve_sign();

  // message header
  size_t tx_idx = tx.get_pos();
  tx.add( (uint8_t)1 ); // pub is only signing account
  tx.add( (uint8_t)0 ); // read-only signed accounts
  tx.add( (uint8_t)2 ); // sysvar and program-id are read-only
                        // unsigned accounts

  auto& first = *upds[ 0 ];

  // accounts
  tx.add_len( n + 4 ); // n + 4 accounts: publish, symbol{n}, sysvar, pyth program, compute budget program
  tx.add( *first.pkey_ ); // publish account
  for ( unsigned i = 0; i < n; ++i ) {
    tx.add( *upds[ i ]->akey_ ); // symbol account
  }
  tx.add( *(pub_key*)sysvar_clock ); // sysvar account
  tx.add( *first.gkey_ ); // programid
  tx.add( *(pub_key*)compute_budget_program_id ); // compute budget program id

  // recent block hash
  tx.add( *first.bhash_ ); // recent block hash

  // instructions section
  unsigned instruction_count = n; // n upd_price instruction(s)
  if ( cu_units > 0 ) {
    instruction_count += 1; // Extra instruction for specifying number of cus per instructions
  }
  if ( cu_price > 0 ) {
    instruction_count += 1; // Extra instruction for specifying compute unit price 
  }
  tx.add_len( instruction_count ); // 1 compute limit instruction, 1 compute unit price instruction, n upd_price instruction(s)

  // Set compute limit
  if ( cu_units > 0 ) {
    tx.add( (uint8_t)( n + 3 ) ); // compute budget program id index in accounts list
    tx.add_len<0>(); // no accounts
    // compute limit instruction parameters
    tx.add_len<sizeof(uint8_t) + sizeof(uint32_t)>(); // uint8_t enum variant + uint32_t requested compute units
    tx.add( (uint8_t) 2 ); // SetComputeLimit enum variant
    tx.add( (uint32_t) ( cu_units * n ) ); // the budget (scaled for number of instructions)
  }

  // Set compute unit price
  if ( cu_price > 0 ) {
    tx.add( (uint8_t)( n + 3 ) );
    tx.add_len<0>(); // no accounts
    // compute unit price instruction parameters
    tx.add_len<sizeof(uint8_t) + sizeof(uint64_t)>(); // uint8_t enum variant + uint62_t compute price
    tx.add( (uint8_t) 3 ); // SetComputePrice enum variant
    tx.add( (uint64_t) cu_price ); // price we are willing to pay per compute unit in Micro Lamports
  }

  for ( unsigned i = 0; i < n; ++i ) {
    tx.add( (uint8_t)( n + 2 ) ); // program_id index
    tx.add_len< 3 >(); // 3 accounts: publish, symbol, sysvar
    tx.add( (uint8_t)0 ); // index of publish account
    tx.add( (uint8_t)( i + 1 ) ); // index of symbol account
    tx.add( (uint8_t)( n + 1 ) ); // index of sysvar account

    auto const& upd = *upds[ i ];

    // instruction parameter section
    tx.add_len<sizeof(cmd_upd_price)>();
    tx.add( (uint32_t)PC_VERSION );
    tx.add( (int32_t)( upd.cmd_ ) );
    tx.add( (int32_t)( upd.st_ ) );
    tx.add( (int32_t)0 );
    tx.add( upd.price_ );
    tx.add( upd.conf_ );
    tx.add( upd.pub_slot_ );
  }

  // all accounts need to sign transaction
  tx.sign( pub_idx, tx_idx, *first.ckey_ );
  first.sig_.init_from_buf( (const uint8_t*)(tx.get_buf() + pub_idx) );

  return true;
}

void rpc::upd_price::build( net_wtr& wtr )
{
  upd_price* upds[] = { this };
  build( wtr, upds, 1 );
}


void rpc::upd_price::request( json_wtr& msg )
{
  upd_price* upds[] = { this };
  request( msg, upds, 1 );
}

bool rpc::upd_price::build(
  net_wtr& wtr, upd_price* upds[], const unsigned n
)
{
  return build( wtr, upds, n, 0, 0 );
}

bool rpc::upd_price::build(
  net_wtr& wtr, upd_price* upds[], const unsigned n, unsigned cu_units, unsigned cu_price
)
{
  bincode tx;
  static_cast< tx_wtr& >( wtr ).init( tx );
  if ( ! build_tx( tx, upds, n, cu_units, cu_price ) ) {
    return false;
  }
  static_cast< tx_wtr& >( wtr ).commit( tx );
  return true;
}

bool rpc::upd_price::request(
  json_wtr& msg, upd_price* upds[], const unsigned n
) {
  return request( msg, upds, n, 0, 0 );
}

bool rpc::upd_price::request(
  json_wtr& msg, upd_price* upds[], const unsigned n, unsigned cu_units, unsigned cu_price
)
{
  // construct binary transaction
  net_buf *bptr = net_buf::alloc();
  bincode tx( bptr->buf_ );
  if ( ! build_tx( tx, upds, n, cu_units, cu_price ) ) {
    return false;
  }

  // encode transaction and add to json params
  msg.add_key( "method", "sendTransaction" );
  msg.add_key( "params", json_wtr::e_arr );
  msg.add_val_enc_base64( str( tx.get_buf(), tx.size() ) );
  msg.add_val( json_wtr::e_obj );
  msg.add_key( "encoding", "base64" );
  msg.add_key( "skipPreflight", json_wtr::jtrue() );
  msg.pop();
  msg.pop();
  bptr->dealloc();

  return true;
}

void rpc::upd_price::response( const jtree& jt )
{
  if ( on_error( jt, this ) )
    return;
  uint32_t rtok = jt.find_val( 1, "result" );
  if ( rtok == 0 )
    return;
  ack_sig_ = jt.get_str( rtok );
  on_response( this );
}
