#include "pyth_client.hpp"
#include "log.hpp"

using namespace pc;

#define PC_RPC_HTTP_PORT      8899
#define PC_RPC_WEBSOCKET_PORT 8900
#define PC_RECONNECT_TIMEOUT  (120L*1000000000L)
#define PC_BLOCKHASH_TIMEOUT  (120L*1000000000L)

///////////////////////////////////////////////////////////////////////////
// pyth_client

void pyth_client::pyth_http::parse_content( const char *txt, size_t len )
{
  ptr_->parse_content( txt, len );
}

pyth_client::pyth_client()
: rptr_( nullptr ),
  sptr_( nullptr )
{
  // setup the plumbing
  hsvr_.ptr_ = this;
  hsvr_.set_net_connect( this );
  hsvr_.set_ws_parser( this );
  set_net_parser( &hsvr_ );
}

void pyth_client::set_rpc_client( rpc_client *rptr )
{
  rptr_ = rptr;
}

void pyth_client::set_pyth_server( pyth_server *sptr )
{
  sptr_ = sptr;
}

void pyth_client::parse_content( const char *, size_t )
{
}

void pyth_client::parse_msg( const char *, size_t )
{
}

void pyth_client::teardown()
{
  // remove self from server list
  sptr_->del_client( this );
}

///////////////////////////////////////////////////////////////////////////
// pyth_request

void pyth_request::set_pyth_server( pyth_server *svr )
{
  svr_ = svr;
}

pyth_server *pyth_request::get_pyth_server() const
{
  return svr_;
}

void pyth_request::set_rpc_client( rpc_client *clnt )
{
  clnt_ = clnt;
}

rpc_client *pyth_request::get_rpc_client() const
{
  return clnt_;
}

///////////////////////////////////////////////////////////////////////////
// pyth_server

pyth_server::pyth_server()
: status_( 0 ),
  cts_( 0L ),
  ctimeout_( PC_NSECS_IN_SEC ),
  btimeout_( PC_BLOCKHASH_TIMEOUT )
{
}

void pyth_server::set_rpc_host( const std::string& rhost )
{
  rhost_ = rhost;
}

std::string pyth_server::get_rpc_host() const
{
  return rhost_;
}

void pyth_server::set_listen_port( int port )
{
  lsvr_.set_port( port );
}

int pyth_server::get_listen_port() const
{
  return lsvr_.get_port();
}

void pyth_server::set_key_file( const std::string& key_file )
{
  kfile_ = key_file;
}

std::string pyth_server::get_key_file() const
{
  return kfile_;
}

key_pair pyth_server::get_key() const
{
  return key_;
}

void pyth_server::set_recent_block_hash_timeout( int64_t btimeout )
{
  btimeout_ = btimeout;
}

int64_t pyth_server::get_recent_block_hash_timeout() const
{
  return btimeout_;
}

hash pyth_server::get_recent_block_hash() const
{
  return bh_->get_block_hash();
}

void pyth_server::teardown()
{
  PC_LOG_INF( "pythd_teardown" ).end();

  // shutdown listener
  lsvr_.close();

  // destroy any open clients
  while( !olist_.empty() ) {
    pyth_client *clnt = olist_.first();
    olist_.del( clnt );
    dlist_.add( clnt );
  }
  teardown_clients();

  // destory rpc connections
  hconn_.close();
  wconn_.close();
}

bool pyth_server::init()
{
  // initialize net_loop
  if ( !nl_.init() ) {
    return set_err_msg( nl_.get_err_msg() );
  }

  // read key file
  if ( !key_.init_from_file( kfile_ ) ) {
    return set_err_msg( "failed to read/parse key_file=" + kfile_ );
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

  // finally initialize listening port
  // dont listen for new clients until we've connected to rpc
  lsvr_.set_net_accept( this );
  lsvr_.set_net_loop( &nl_ );
  if ( !lsvr_.init() ) {
    return set_err_msg( lsvr_.get_err_msg() );
  }

  // setup recent block hash
  bh_->set_sub( this );

  return true;
}

bool pyth_server::has_status( int status ) const
{
  return status == (status_ & status);
}

void pyth_server::set_status( int status )
{
  status_ |= status;
}

void pyth_server::reset_status( int status )
{
  status_ &= ~status;
}

void pyth_server::poll()
{
  // poll for any socket events
  nl_.poll( 1 );

  // submit pending requests
  while( !plist_.empty() &&
         has_status( PC_PYTH_RPC_CONNECTED |
                     PC_PYTH_HAS_BLOCK_HASH ) ) {
    pyth_request *rptr = plist_.first();
    rptr->submit();
    plist_.del( rptr );
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

void pyth_server::reconnect_rpc()
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
  } else {
    log_disconnect();
  }
}

void pyth_server::log_disconnect()
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

void pyth_server::teardown_clients()
{
  while( !dlist_.empty() ) {
    pyth_client *clnt = dlist_.first();
    PC_LOG_INF( "delete_client" ).add("fd", clnt->get_fd() ).end();
    clnt->close();
    dlist_.del( clnt );
  }
}

void pyth_server::accept( int fd )
{
  // create and add new pyth_client
  pyth_client *clnt = new pyth_client;
  clnt->set_rpc_client( &clnt_ );
  clnt->set_net_loop( &nl_ );
  clnt->set_pyth_server( this );
  clnt->set_fd( fd );
  clnt->set_block( false );
  if ( clnt->init() ) {
    PC_LOG_INF( "new_client" ).add("fd", fd ).end();
    olist_.add( clnt );
  } else {
    clnt->close();
    delete clnt;
  }
}

void pyth_server::del_client( pyth_client *clnt )
{
  // move clnt from open clist to delete list
  olist_.del( clnt );
  dlist_.add( clnt );
}

void pyth_server::on_response( rpc::get_recent_block_hash *m )
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

void pyth_server::submit( pyth_request *req )
{
  req->set_pyth_server( this );
  req->set_rpc_client( &clnt_ );
  plist_.add( req );
}

///////////////////////////////////////////////////////////////////////////
// pyth::create_mapping

pyth::create_mapping::create_mapping()
: st_( e_new )
{
}

void pyth::create_mapping::submit()
{
  // initialize state
  st_ = e_new;

  // get recent block hash and submit request
  req_->set_block_hash( get_pyth_server()->get_recent_block_hash() );
  req_->set_sub( this );
  sig_->set_sub( this );
  get_rpc_client()->send( req_ );
  st_ = e_create_sent;
}

rpc::create_account *pyth::create_mapping::get_create_account()
{
  return req_;
}

void pyth::create_mapping::on_response( rpc::create_account *res )
{
  if ( res->get_is_err() ) {
    set_err_msg( res->get_err_msg() );
    st_ = e_error;
  } else if ( st_ == e_create_sent ) {
    // subscribe to signature completion
    st_ = e_create_done;
    sig_->set_signature( res->get_fund_signature() );
    get_rpc_client()->send( sig_ );
  }
}

void pyth::create_mapping::on_response( rpc::signature_subscribe *res )
{
  if ( res->get_is_err() ) {
    set_err_msg( res->get_err_msg() );
    st_ = e_error;
  } else if ( st_ == e_create_done ) {
    // submit account init
  }
}
