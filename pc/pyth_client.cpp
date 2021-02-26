#include "pyth_client.hpp"

using namespace pc;

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
: lptr_( nullptr )
{
}

void pyth_server::teardown()
{
  // shutdown listener
  lptr_->close();

  // destroy any open clients
  for(;;) {
    pyth_client *clnt = olist_.first();
    if ( clnt ) {
      clnt->close();
      olist_.del( clnt );
      delete clnt;
    } else {
      break;
    }
  }
  // destory rpc connections
  get_rpc_http_conn()->close();
  get_rpc_ws_conn()->close();
}

void pyth_server::set_rpc_http_conn( net_connect * hptr )
{
  clnt_.set_http_conn( hptr );
}

net_connect *pyth_server::get_rpc_http_conn() const
{
  return clnt_.get_http_conn();
}

void pyth_server::set_rpc_ws_conn( net_connect *wptr )
{
  clnt_.set_ws_conn( wptr );
}

net_connect *pyth_server::get_rpc_ws_conn() const
{
  return clnt_.get_ws_conn();
}

void pyth_server::set_listen( net_listen *lptr )
{
  lptr_ = lptr;
}

bool pyth_server::init()
{
  // initialize net_loop
  if ( !nl_.init() ) {
    return set_err_msg( nl_.get_err_msg() );
  }

  // add rpc_client connections to net_loop and initialize
  net_connect *hptr = get_rpc_http_conn();
  hptr->set_net_loop( &nl_ );
  if ( !hptr->init() ) {
    return set_err_msg( hptr->get_err_msg() );
  }
  net_connect *wptr = get_rpc_http_conn();
  wptr->set_net_loop( &nl_ );
  if ( !wptr->init() ) {
    return set_err_msg( wptr->get_err_msg() );
  }

  // finally initialize listeniing port
  // dont listen for new clients until we've connected to rpc
  lptr_->set_net_accept( this );
  lptr_->set_net_loop( &nl_ );
  if ( !lptr_->init() ) {
    return set_err_msg( lptr_->get_err_msg() );
  }

  return true;
}

void pyth_server::poll()
{
  // poll for any socket events
  nl_.poll( 1 );

  // destroy any clients scheduled for deletion
  for(;;) {
    pyth_client *clnt = dlist_.first();
    if ( clnt ) {
      clnt->close();
      dlist_.del( clnt );
      delete clnt;
    } else {
      break;
    }
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
