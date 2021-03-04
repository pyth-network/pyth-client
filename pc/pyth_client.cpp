#include "pyth_client.hpp"
#include "log.hpp"

using namespace pc;

#define PC_RPC_HTTP_PORT      8899
#define PC_RPC_WEBSOCKET_PORT 8900
#define PC_RECONNECT_TIMEOUT  (120L*1000000000L)
#define PC_BLOCKHASH_TIMEOUT  (2L*1000000000L)

#define PC_JSON_RPC_VER         "2.0"
#define PC_JSON_PARSE_ERROR     -32700
#define PC_JSON_INVALID_REQUEST -32600
#define PC_JSON_UNKNOWN_METHOD  -32601
#define PC_JSON_INVALID_PARAMS  -32602
#define PC_JSON_UNKNOWN_SYMBOL  -32000

///////////////////////////////////////////////////////////////////////////
// pyth_user

void pyth_user::pyth_http::parse_content( const char *txt, size_t len )
{
  ptr_->parse_content( txt, len );
}

pyth_user::pyth_user()
: rptr_( nullptr ),
  sptr_( nullptr )
{
  // setup the plumbing
  hsvr_.ptr_ = this;
  hsvr_.set_net_connect( this );
  hsvr_.set_ws_parser( this );
  set_net_parser( &hsvr_ );
}

void pyth_user::set_rpc_client( rpc_client *rptr )
{
  rptr_ = rptr;
}

void pyth_user::set_pyth_client( pyth_client *sptr )
{
  sptr_ = sptr;
}

void pyth_user::teardown()
{
  // remove self from server list
  sptr_->del_client( this );
}

void pyth_user::parse_content( const char *, size_t )
{
}

void pyth_user::parse_msg( const char *txt, size_t len )
{
  jp_.parse( txt, len );
  jw_.reset();
  if ( jp_.is_valid() ) {
    jtree::type_t t = jp_.get_type( 1 );
    if ( t == jtree::e_obj ) {
      parse_request( 1 );
    } else if ( t == jtree::e_arr ) {
      uint32_t tok = jp_.get_first( 1 );
      if ( tok ) {
        jw_.add( json_wtr::e_arr );
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
}

void pyth_user::parse_request( uint32_t tok )
{
  uint32_t itok = jp_.find_val( tok, "id" );
  uint32_t mtok = jp_.find_val( tok, "method" );
  if ( itok ==0 || mtok == 0 ) {
    add_invalid_request( itok );
    return;
  }
  str mst = jp_.get_str( mtok );
  if ( mst == "upd_price" ) {
    parse_upd_price( tok, itok );
  } else {
    add_error( itok, PC_JSON_UNKNOWN_METHOD, "method not found" );
  }
}

void pyth_user::parse_upd_price( uint32_t tok, uint32_t itok )
{
  // unpack parameters
  uint32_t ptok = jp_.find_val( tok, "params" );
  if ( PC_UNLIKELY( ptok == 0 || jp_.get_type( ptok ) != jtree::e_arr )) {
    add_invalid_params( itok );
    return;
  }
  uint32_t stok = jp_.get_first( ptok );
  uint32_t xtok = stok ? jp_.get_next( stok ) : 0;
  uint32_t ctok = xtok ? jp_.get_next( xtok ) : 0;
  if ( PC_UNLIKELY( stok == 0 || xtok == 0 || ctok == 0 ) ) {
    add_invalid_params( itok );
    return;
  }
  symbol sym = jp_.get_str( stok );
  pyth_symbol_price *sptr = sptr_->get_symbol_price( sym );
  if ( PC_UNLIKELY( !sptr ) ) {
    add_unknown_symbol( itok );
    return;
  }

  // submit new price
  int64_t price = jp_.get_int( xtok );
  uint64_t conf = jp_.get_uint( ctok );
  sptr->upd_price( price, conf );

  // create result
  add_header();
  jw_.add_key( "result", 0UL );
  add_tail( itok );
}

void pyth_user::add_header()
{
  jw_.add_val( json_wtr::e_obj );
  jw_.add_key( "jsonrpc", str( PC_JSON_RPC_VER ) );
}

void pyth_user::add_tail( uint32_t id )
{
  // echo back id using same type as that provided
  if ( id ) {
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

void pyth_user::add_unknown_symbol( uint32_t itok )
{
  add_error( itok, PC_JSON_UNKNOWN_SYMBOL, "unknown symbol" );
}

void pyth_user::add_parse_error()
{
  add_error( 0, PC_JSON_PARSE_ERROR, "parse error" );
}

void pyth_user::add_invalid_params( uint32_t itok )
{
  add_error( itok, PC_JSON_INVALID_PARAMS, "invalid params" );
}

void pyth_user::add_invalid_request( uint32_t itok )
{
  add_error( itok, PC_JSON_INVALID_REQUEST, "invalid request" );
}

void pyth_user::add_error( uint32_t id, int err, str emsg )
{
  // send error message back to client
  add_header();
  jw_.add_key( "error", json_wtr::e_obj );
  jw_.add_key( "code", (int64_t)err );
  jw_.add_key( "message", str( emsg ) );
  jw_.pop();
  add_tail( id  );
}

///////////////////////////////////////////////////////////////////////////
// pyth_client

pyth_client::pyth_client()
: status_( 0 ),
  cts_( 0L ),
  ctimeout_( PC_NSECS_IN_SEC ),
  btimeout_( PC_BLOCKHASH_TIMEOUT )
{
}

pyth_client::~pyth_client()
{
  for( pyth_get_mapping *mptr: mvec_ ) {
    delete mptr;
  }
  mvec_.clear();
  for( pyth_symbol_price *ptr: svec_ ) {
    delete ptr;
  }
  svec_.clear();
}

void pyth_client::set_rpc_host( const std::string& rhost )
{
  rhost_ = rhost;
}

std::string pyth_client::get_rpc_host() const
{
  return rhost_;
}

void pyth_client::set_listen_port( int port )
{
  lsvr_.set_port( port );
}

int pyth_client::get_listen_port() const
{
  return lsvr_.get_port();
}

void pyth_client::set_recent_block_hash_timeout( int64_t btimeout )
{
  btimeout_ = btimeout;
}

int64_t pyth_client::get_recent_block_hash_timeout() const
{
  return btimeout_;
}

hash *pyth_client::get_recent_block_hash()
{
  return bh_->get_block_hash();
}

void pyth_client::teardown()
{
  PC_LOG_INF( "pythd_teardown" ).end();

  // shutdown listener
  lsvr_.close();

  // destroy any open clients
  while( !olist_.empty() ) {
    pyth_user *usr = olist_.first();
    olist_.del( usr );
    dlist_.add( usr );
  }
  teardown_clients();

  // destory rpc connections
  hconn_.close();
  wconn_.close();
}

bool pyth_client::init()
{
  // read key directory
  if ( !key_store::init() ) {
    return false;
  }

  // build mapping cache if mapping key defined
  init_mapping();

  // initialize net_loop
  if ( !nl_.init() ) {
    return set_err_msg( nl_.get_err_msg() );
  }

  // add rpc_client connection to net_loop and initialize
  hconn_.set_port( PC_RPC_HTTP_PORT );
  hconn_.set_host( rhost_ );
  hconn_.set_net_loop( &nl_ );
  clnt_.set_http_conn( &hconn_ );
  wconn_.set_port( PC_RPC_WEBSOCKET_PORT );
  wconn_.set_host( rhost_ );
  wconn_.set_net_loop( &nl_ );
  clnt_.set_ws_conn( &wconn_ );
  if ( hconn_.init() && wconn_.init() ) {
    PC_LOG_INF( "rpc_connected" ).end();
    set_status( PC_PYTH_RPC_CONNECTED );
  }

  // initialize listening port if port defined
  if ( lsvr_.get_port() > 0 ) {
    lsvr_.set_net_accept( this );
    lsvr_.set_net_loop( &nl_ );
    if ( !lsvr_.init() ) {
      return set_err_msg( lsvr_.get_err_msg() );
    }
  }

  // setup recent block hash
  bh_->set_sub( this );

  return true;
}

void pyth_client::init_mapping()
{
  // log publishing key name
  key_pair *kp = get_publish_key_pair();
  if ( kp ) {
    PC_LOG_INF( "publish_key" ).add( "key_name", *kp ).end();
  }
  pub_key *mpub = get_mapping_pub_key();
  if ( !mpub ) {
    return;
  }
  PC_LOG_INF( "mapping_key" ).add( "key_name", *mpub ).end();
  pyth_get_mapping *mptr = new pyth_get_mapping;
  mptr->set_mapping_key( mpub );
  mvec_.push_back( mptr );
  submit( mptr );
}

bool pyth_client::has_status( int status ) const
{
  return status == (status_ & status);
}

void pyth_client::set_status( int status )
{
  status_ |= status;
}

void pyth_client::reset_status( int status )
{
  status_ &= ~status;
}

void pyth_client::poll()
{
  // poll for any socket events
  nl_.poll( 1 );

  // submit pending requests
  for( pyth_request *rptr =plist_.first(); rptr; ) {
    pyth_request *nxt = rptr->get_next();
    if ( rptr->has_status() ) {
      plist_.del( rptr );
      rptr->submit();
    }
    rptr = nxt;
  }

  // check if we need to reconnect rpc services
  if ( PC_UNLIKELY( !has_status( PC_PYTH_RPC_CONNECTED ) ||
                    hconn_.get_is_err() || wconn_.get_is_err() ))  {
    reconnect_rpc();
    return;
  }

  // periodically submit recent block hash request
  int64_t ts = get_now();
  if ( PC_UNLIKELY( has_status( PC_PYTH_RPC_CONNECTED ) &&
                    ts > bh_->get_sent_time() + btimeout_ &&
                    bh_->get_is_recv() ) ) {
    clnt_.send( bh_ );
  }

  // destroy any clients scheduled for deletion
  teardown_clients();
}

void pyth_client::reconnect_rpc()
{
  // log disconnect error
  if ( has_status( PC_PYTH_RPC_CONNECTED ) ) {
    log_disconnect();
  }
  reset_status( PC_PYTH_RPC_CONNECTED );
  int64_t ts = get_now();
  if ( ctimeout_ > (ts-cts_) ) {
    return;
  }
  // attempt to reconnect
  cts_ = ts;
  ctimeout_ += ctimeout_;
  ctimeout_ = std::min( ctimeout_, PC_RECONNECT_TIMEOUT );
  if ( hconn_.init() && wconn_.init() ) {
    PC_LOG_INF( "rpc_connected" ).end();
    set_status( PC_PYTH_RPC_CONNECTED );
    ctimeout_ = PC_NSECS_IN_SEC;
    bh_->set_sent_time( 0L );

    // resubscribe to pyth mapping and symbols
    for( pyth_get_mapping *mptr: mvec_ ) {
      mptr->submit();
    }
    for( pyth_symbol_price *ptr: svec_ ) {
      ptr->reset();
      ptr->submit();
    }
  } else {
    log_disconnect();
  }
}

void pyth_client::log_disconnect()
{
  if ( hconn_.get_is_err() ) {
    PC_LOG_ERR( "rpc_http_reset")
      .add( "error", hconn_.get_err_msg() )
      .add( "host", rhost_ )
      .add( "port", hconn_.get_port() )
      .end();
    return;
  }
  if ( wconn_.get_is_err() ) {
    PC_LOG_ERR( "rpc_websocket_reset" )
      .add( "error", wconn_.get_err_msg() )
      .add( "host", rhost_ )
      .add( "port", wconn_.get_port() )
      .end();
    return;
  }
}

void pyth_client::teardown_clients()
{
  while( !dlist_.empty() ) {
    pyth_user *usr = dlist_.first();
    PC_LOG_INF( "delete_client" ).add("fd", usr->get_fd() ).end();
    usr->close();
    dlist_.del( usr );
  }
}

void pyth_client::accept( int fd )
{
  // create and add new pyth_user
  pyth_user *usr = new pyth_user;
  usr->set_rpc_client( &clnt_ );
  usr->set_net_loop( &nl_ );
  usr->set_pyth_client( this );
  usr->set_fd( fd );
  usr->set_block( false );
  if ( usr->init() ) {
    PC_LOG_INF( "new_client" ).add("fd", fd ).end();
    olist_.add( usr );
  } else {
    usr->close();
    delete usr;
  }
}

void pyth_client::del_client( pyth_user *usr )
{
  // move usr from open clist to delete list
  olist_.del( usr );
  dlist_.add( usr );
}

void pyth_client::on_response( rpc::get_recent_block_hash *m )
{
  if ( !m->get_is_err() ) {
    set_status( PC_PYTH_HAS_BLOCK_HASH );
  } else  {
    /*
    PC_LOG_ERR( "failed_to_get_recent_block_hash" )
      .add( "err_code", m->get_err_code())
      .add( "err_msg", m->get_err_msg() )
      .end();
      */
    reset_status( PC_PYTH_HAS_BLOCK_HASH );
  }
}

void pyth_client::submit( pyth_request *req )
{
  req->set_pyth_client( this );
  req->set_rpc_client( &clnt_ );
  plist_.add( req );
}

void pyth_client::add_symbol( const symbol& sym, const pub_key&acc )
{
  sym_map_t::iter_t i = smap_.find( sym );
  if ( !i ) {
    i = smap_.add( sym );
    pyth_symbol_price *ptr = new pyth_symbol_price( sym, acc );
    smap_.ref( i ) = ptr;
    svec_.push_back( ptr );
    PC_LOG_INF( "new_symbol" )
      .add( "symbol", sym )
      .add( "account", acc )
      .end();
    submit( ptr );
  }
}

pyth_symbol_price *pyth_client::get_symbol_price( const symbol& sym )
{
  sym_map_t::iter_t i = smap_.find( sym );
  if ( i ) {
    return smap_.obj( i );
  } else {
    return nullptr;
  }
}
