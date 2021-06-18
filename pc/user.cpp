#include "user.hpp"
#include "manager.hpp"
#include "log.hpp"
#include "mem_map.hpp"

#define PC_JSON_RPC_VER         "2.0"
#define PC_JSON_PARSE_ERROR     -32700
#define PC_JSON_INVALID_REQUEST -32600
#define PC_JSON_UNKNOWN_METHOD  -32601
#define PC_JSON_INVALID_PARAMS  -32602
#define PC_JSON_UNKNOWN_SYMBOL  -32000
#define PC_JSON_MISSING_PERMS   -32001
#define PC_JSON_NOT_READY       -32002

using namespace pc;

///////////////////////////////////////////////////////////////////////////
// user

void user::user_http::parse_content( const char *txt, size_t len )
{
  ptr_->parse_content( txt, len );
}

user::user()
: rptr_( nullptr ),
  sptr_( nullptr ),
  psub_( this )
{
  // setup the plumbing
  hsvr_.ptr_ = this;
  hsvr_.set_net_connect( this );
  hsvr_.set_ws_parser( this );
  set_net_parser( &hsvr_ );
}

void user::set_rpc_client( rpc_client *rptr )
{
  rptr_ = rptr;
}

void user::set_manager( manager *sptr )
{
  sptr_ = sptr;
}

void user::teardown()
{
  net_connect::teardown();

  // remove self from server list
  sptr_->del_user( this );

  // remove all symbol subscriptions
  psub_.teardown();
}

static str get_content_type( const std::string & filen )
{
  size_t i = filen.find_last_of( '.' );
  if ( i != std::string::npos ) {
    std::string ext = filen.substr( i );
    if ( ext == ".html" ) {
      return "text/html";
    } else if ( ext == ".css" ) {
      return "text/css";
    } else if ( ext == ".js" ) {
      return "application/javascript";
    }
  }
  return "text/html";
}

void user::parse_content( const char *, size_t )
{
  str path;
  hsvr_.get_path( path );
  std::string cfile = sptr_->get_content_dir();
  if ( cfile.empty() ) {
    cfile += ".";
  }
  cfile += std::string( path.str_, path.len_ );
  if ( path == str( "/" ) ) {
    cfile += "index.html";
  }
  mem_map mf;
  mf.set_file( cfile );
  http_response msg;
  if ( mf.init() ) {
    msg.init( "200", "OK" );
    msg.add_hdr( "Content-Type", get_content_type( cfile ) );
    net_wtr mfb;
    mfb.add( str( mf.data(), mf.size() ) );
    msg.commit( mfb );
  } else {
    msg.init( "404", "Not Found" );
    msg.commit();
  }
  add_send( msg );
}

void user::parse_msg( const char *txt, size_t len )
{
  jw_.reset();
  jp_.parse( txt, len );
  if ( jp_.is_valid() ) {
    jtree::type_t t = jp_.get_type( 1 );
    if ( t == jtree::e_obj ) {
      parse_request( 1 );
    } else if ( t == jtree::e_arr ) {
      uint32_t tok = jp_.get_first( 1 );
      if ( tok ) {
        jw_.add_val( json_wtr::e_arr );
        for( ; tok; tok = jp_.get_next( tok ) ) {
          if ( jp_.get_type( tok ) == jtree::e_obj ) {
            parse_request( tok );
          } else {
            add_invalid_request();
          }
        }
        jw_.pop();
      } else {
        add_invalid_request();
      }
    } else {
      add_invalid_request();
    }
  } else {
    add_parse_error();
  }
  // wrap in websockets header and submit
  ws_wtr msg;
  msg.commit( ws_wtr::text_id, jw_, false );
  add_send( msg );

  // process any deferred subscriptions
  if ( PC_UNLIKELY( !dvec_.empty() ) ) {
    for( deferred_sub& dsub: dvec_ ) {
      on_response( dsub.sptr_, dsub.sid_ );
    }
    dvec_.clear();
  }
}

void user::parse_request( uint32_t tok )
{
  uint32_t itok = jp_.find_val( tok, "id" );
  uint32_t mtok = jp_.find_val( tok, "method" );
  if ( itok ==0 || mtok == 0 ||
       jtree::e_val != jp_.get_type( itok ) ||
       jtree::e_val != jp_.get_type( mtok ) ) {
    add_invalid_request( itok );
    return;
  }
  str mst = jp_.get_str( mtok );
  if ( mst == "update_price" ) {
    parse_upd_price( tok, itok );
  } else if ( mst == "subscribe_price" ) {
    parse_sub_price( tok, itok );
  } else if ( mst == "subscribe_price_sched" ) {
    parse_sub_price_sched( tok, itok );
  } else if ( mst == "get_product_list" ) {
    parse_get_product_list( itok );
  } else {
    add_error( itok, PC_JSON_UNKNOWN_METHOD, "method not found" );
  }
}

void user::parse_upd_price( uint32_t tok, uint32_t itok )
{
  do {
    // unpack and verify parameters
    uint32_t ntok,ptok = jp_.find_val( tok, "params" );
    if ( ptok == 0 || jp_.get_type(ptok) != jtree::e_obj ) break;
    if ( 0 == (ntok = jp_.find_val( ptok, "account" ) ) ) break;
    pub_key pkey;
    pkey.init_from_text( jp_.get_str( ntok ) );
    price *sptr = sptr_->get_price( pkey );
    if ( PC_UNLIKELY( !sptr ) ) { add_unknown_symbol(itok); return; }
    if ( 0 == (ntok = jp_.find_val( ptok, "price" ) ) ) break;
    int64_t price = jp_.get_int( ntok );
    if ( 0 == (ntok = jp_.find_val( ptok, "conf" ) ) ) break;
    uint64_t conf = jp_.get_uint( ntok );
    if ( 0 == (ntok = jp_.find_val( ptok, "status" ) ) ) break;
    symbol_status stype = str_to_symbol_status( jp_.get_str( ntok ) );

    // submit new price
    if ( sptr->update( price, conf, stype ) ) {
      // create result
      add_header();
      jw_.add_key( "result", 0UL );
      add_tail( itok );
    } else if ( !sptr->get_is_ready_publish() ) {
      add_error( itok, PC_JSON_NOT_READY,
          "not ready to publish - check pyth_tx connection" );
    } else if ( !sptr->has_publisher() ) {
      add_error( itok, PC_JSON_MISSING_PERMS, "missing publish permission" );
    } else if ( sptr->get_is_err() ) {
      add_error( itok, PC_JSON_INVALID_REQUEST, sptr->get_err_msg() );
    } else {
      add_error( itok, PC_JSON_INVALID_REQUEST, "unknown error" );
    }
    return;
  } while( 0 );
  add_invalid_params( itok );
}

void user::parse_sub_price( uint32_t tok, uint32_t itok )
{
  do {
    // unpack and verify parameters
    uint32_t ntok,ptok = jp_.find_val( tok, "params" );
    if ( ptok == 0 || jp_.get_type(ptok) != jtree::e_obj ) break;
    if ( 0 == (ntok = jp_.find_val( ptok, "account" ) ) ) break;
    pub_key pkey;
    pkey.init_from_text( jp_.get_str( ntok ) );
    price *sptr = sptr_->get_price( pkey );
    if ( PC_UNLIKELY( !sptr ) ) { add_unknown_symbol(itok); return; }

    // add subscription
    uint64_t sub_id = psub_.add( sptr );

    // create result
    add_header();
    jw_.add_key( "result", json_wtr::e_obj );
    jw_.add_key( "subscription", sub_id );
    jw_.pop();
    add_tail( itok );

    // add subscription
    deferred_sub dsub{ sptr, sub_id };
    dvec_.push_back( dsub );
    return;
  } while( 0 );
  add_invalid_params( itok );
}

void user::parse_sub_price_sched( uint32_t tok, uint32_t itok )
{
  do {
    // unpack and verify parameters
    uint32_t ntok,ptok = jp_.find_val( tok, "params" );
    if ( ptok == 0 || jp_.get_type(ptok) != jtree::e_obj ) break;
    if ( 0 == (ntok = jp_.find_val( ptok, "account" ) ) ) break;
    pub_key pkey;
    pkey.init_from_text( jp_.get_str( ntok ) );
    price *sptr = sptr_->get_price( pkey );
    if ( PC_UNLIKELY( !sptr ) ) { add_unknown_symbol(itok); return; }

    // add subscription
    uint64_t sub_id = psub_.add( sptr->get_sched() );

    // create result
    add_header();
    jw_.add_key( "result", json_wtr::e_obj );
    jw_.add_key( "subscription", sub_id );
    jw_.pop();
    add_tail( itok );
    return;
  } while( 0 );
  add_invalid_params( itok );
}

void user::parse_get_product_list( uint32_t itok )
{
  add_header();
  jw_.add_key( "result", json_wtr::e_arr );
  for( unsigned i=0; i != sptr_->get_num_product(); ++i ) {
    product *prod = sptr_->get_product( i );
    jw_.add_val( json_wtr::e_obj );
    jw_.add_key( "account", *prod->get_account() );
    jw_.add_key( "attr_dict", json_wtr::e_obj );
    prod->write_json( jw_ );
    jw_.pop();
    jw_.add_key( "price", json_wtr::e_arr );
    for( unsigned j=0; j != prod->get_num_price(); ++j ) {
      jw_.add_val( json_wtr::e_obj );
      price *px = prod->get_price( j );
      int64_t expo = px->get_price_exponent();
      price_type ptype = px->get_price_type();
      jw_.add_key( "account", *px->get_account() );
      jw_.add_key( "price_exponent", expo );
      jw_.add_key( "price_type", price_type_to_str( ptype) );
      jw_.pop();
    }
    jw_.pop();
    jw_.pop();
  }
  jw_.pop();
  add_tail( itok );
}

void user::add_header()
{
  jw_.add_val( json_wtr::e_obj );
  jw_.add_key( "jsonrpc", str( PC_JSON_RPC_VER ) );
}

void user::add_tail( uint32_t id )
{
  // echo back id using same type as that provided
  if ( id && jtree::e_val == jp_.get_type(id) ) {
    str idv = jp_.get_str( id );
    if ( idv.str_[-1] == '"' ) {
      jw_.add_key( "id", idv );
    } else {
      jw_.add_key_verbatim( "id", idv );
    }
  } else {
    jw_.add_key( "id", json_wtr::null() );
  }
  // pop enclosing object block
  jw_.pop();
}

void user::add_unknown_symbol( uint32_t itok )
{
  add_error( itok, PC_JSON_UNKNOWN_SYMBOL, "unknown symbol" );
}

void user::add_parse_error()
{
  add_error( 0, PC_JSON_PARSE_ERROR, "parse error" );
}

void user::add_invalid_params( uint32_t itok )
{
  add_error( itok, PC_JSON_INVALID_PARAMS, "invalid params" );
}

void user::add_invalid_request( uint32_t itok )
{
  add_error( itok, PC_JSON_INVALID_REQUEST, "invalid request" );
}

void user::add_error( uint32_t id, int err, str emsg )
{
  // send error message back to user
  add_header();
  jw_.add_key( "error", json_wtr::e_obj );
  jw_.add_key( "code", (int64_t)err );
  jw_.add_key( "message", str( emsg ) );
  jw_.pop();
  add_tail( id  );
}

void user::on_response( price *rptr, uint64_t idx )
{
  // construct notify response
  jw_.reset();
  add_header();
  jw_.add_key( "method", "notify_price" );
  jw_.add_key( "params", json_wtr::e_obj );
  jw_.add_key( "result", json_wtr::e_obj );
  jw_.add_key( "price", rptr->get_price() );
  jw_.add_key( "conf", rptr->get_conf() );
  jw_.add_key( "twap", rptr->get_twap() );
  jw_.add_key( "twac", rptr->get_twac() );
  jw_.add_key( "status", symbol_status_to_str( rptr->get_status() ) );
  jw_.add_key( "valid_slot", rptr->get_valid_slot() );
  jw_.add_key( "pub_slot", rptr->get_pub_slot() );
  jw_.pop();
  jw_.add_key( "subscription", idx );
  jw_.pop();
  jw_.pop();

  // wrap in websockets header and submit
  ws_wtr msg;
  msg.commit( ws_wtr::text_id, jw_, false );
  add_send( msg );
}

void user::on_response( price_sched *, uint64_t idx )
{
  // construct notify response
  jw_.reset();
  add_header();
  jw_.add_key( "method", "notify_price_sched" );
  jw_.add_key( "params", json_wtr::e_obj );
  jw_.add_key( "subscription", idx );
  jw_.pop();
  jw_.pop();

  // wrap in websockets header and submit
  ws_wtr msg;
  msg.commit( ws_wtr::text_id, jw_, false );
  add_send( msg );
}
