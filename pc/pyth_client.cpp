#include "pyth_client.hpp"

using namespace pc;

#define PC_RPC_HTTP_PORT      8899
#define PC_RPC_WEBSOCKET_PORT 8900

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
// pyth_server

pyth_server::pyth_server()
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

void pyth_server::teardown()
{
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

  // add rpc_client connection to net_loop and initialize
  hconn_.set_port( PC_RPC_HTTP_PORT );
  hconn_.set_host( rhost_ );
  hconn_.set_net_loop( &nl_ );
  clnt_.set_http_conn( &hconn_ );
  if ( !hconn_.init() ) {
    return set_err_msg( hconn_.get_err_msg() );
  }

  // add rpc_client websocket connection to net_loop and initialize
  wconn_.set_port( PC_RPC_WEBSOCKET_PORT );
  wconn_.set_host( rhost_ );
  wconn_.set_net_loop( &nl_ );
  clnt_.set_ws_conn( &wconn_ );
  if ( !wconn_.init() ) {
    return set_err_msg( wconn_.get_err_msg() );
  }

  // finally initialize listeniing port
  // dont listen for new clients until we've connected to rpc
  lsvr_.set_net_accept( this );
  lsvr_.set_net_loop( &nl_ );
  if ( !lsvr_.init() ) {
    return set_err_msg( lsvr_.get_err_msg() );
  }

  return true;
}

void pyth_server::poll()
{
  // poll for any socket events
  nl_.poll( 1 );

  // TODO: check if we need to reconnect rpc connections

  // destroy any clients scheduled for deletion
  teardown_clients();
}

void pyth_server::teardown_clients()
{
  while( !dlist_.empty() ) {
    pyth_client *clnt = dlist_.first();
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
