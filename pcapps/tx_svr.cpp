#include "tx_svr.hpp"
#include <pc/log.hpp>

#define PC_TPU_PROXY_PORT     8898
#define PC_RPC_HTTP_PORT      8899
#define PC_LEADER_MAX         256
#define PC_LEADER_MIN         32
#define PC_RECONNECT_TIMEOUT  (120L*1000000000L)
#define PC_HBEAT_INTERVAL     16

using namespace pc;

///////////////////////////////////////////////////////////////////////////
// tx_user

tx_user::tx_user()
{
  set_net_parser( this );
}

void tx_user::set_tx_svr( tx_svr *mgr )
{
  mgr_ = mgr;
}

bool tx_user::parse( const char *buf, size_t sz, size_t&len  )
{
  tx_hdr *hdr = (tx_hdr*)buf;
  if ( PC_UNLIKELY( sz < sizeof( tx_hdr) || sz < hdr->size_ ) ) {
    return false;
  }
  if ( PC_UNLIKELY( hdr->proto_id_ != PC_TPU_PROTO_ID ) ) {
    teardown();
    return false;
  }
  mgr_->submit( (const char*)&hdr[1], hdr->size_ - sizeof( tx_hdr ) );
  len = hdr->size_;
  return true;
}

void tx_user::teardown()
{
  net_connect::teardown();

  // remove self from server list
  mgr_->del_user( this );
}

///////////////////////////////////////////////////////////////////////////
// tx_svr

tx_svr::tx_svr()
: has_conn_( false ),
  wait_conn_( false ),
  msg_( new char[buf_len] ),
  slot_( 0UL ),
  slot_cnt_( 0UL ),
  cts_( 0L ),
  ctimeout_( PC_NSECS_IN_SEC )
{
  hreq_->set_sub( this );
  sreq_->set_sub( this );
  creq_->set_sub( this );
  lreq_->set_sub( this );
  lreq_->set_limit( PC_LEADER_MAX );
}

tx_svr::~tx_svr()
{
  teardown();
  delete [] msg_;
  msg_ = nullptr;
}

void tx_svr::set_rpc_host( const std::string& rhost )
{
  rhost_ = rhost;
}

std::string tx_svr::get_rpc_host() const
{
  return rhost_;
}

void tx_svr::set_listen_port( int port )
{
  tsvr_.set_port( port );
}

int tx_svr::get_listen_port() const
{
  return tsvr_.get_port();
}

bool tx_svr::init()
{
  // initialize net_loop
  if ( !nl_.init() ) {
    return set_err_msg( nl_.get_err_msg() );
  }

  // decompose rpc_host into host:port[:port2]
  int rport =0, wport = 0;
  std::string rhost = get_host_port( rhost_, rport, wport );
  if ( rport == 0 ) rport = PC_RPC_HTTP_PORT;
  if ( wport == 0 ) wport = rport+1;

  // add rpc_client connection to net_loop and initialize
  hconn_.set_port( rport );
  hconn_.set_host( rhost );
  hconn_.set_net_loop( &nl_ );
  clnt_.set_http_conn( &hconn_ );
  wconn_.set_port( wport );
  wconn_.set_host( rhost );
  wconn_.set_net_loop( &nl_ );
  clnt_.set_ws_conn( &wconn_ );
  if ( !hconn_.init() ) {
    return set_err_msg( hconn_.get_err_msg() );
  }
  if ( !wconn_.init() ) {
    return set_err_msg( wconn_.get_err_msg() );
  }
  if ( !tconn_.init() ) {
    return set_err_msg( tconn_.get_err_msg() );
  }
  tsvr_.set_net_accept( this );
  tsvr_.set_net_loop( &nl_ );
  if ( !tsvr_.init() ) {
    return set_err_msg( tsvr_.get_err_msg() );
  }
  PC_LOG_INF("initialized")
    .add("listen_port",tsvr_.get_port())
    .add("rpc_host", rhost )
    .end();
  wait_conn_ = true;
  return true;
}

void tx_svr::poll( bool do_wait )
{
  // epoll loop
  if ( do_wait ) {
    nl_.poll( 1 );
  } else {
    if ( has_conn_ ) {
      hconn_.poll();
      wconn_.poll();
    }
    tsvr_.poll();
    for( tx_user *uptr = olist_.first(); uptr; ) {
      tx_user *nptr = uptr->get_next();
      uptr->poll();
      uptr = nptr;
    }
  }

  // destroy any users scheduled for deletion
  teardown_users();

  // reconnect to rpc as required
  if ( PC_UNLIKELY( !has_conn_ ||
        hconn_.get_is_err() || wconn_.get_is_err() ) ) {
    reconnect_rpc();
  }
}

void tx_svr::del_user( tx_user *usr )
{
  // move usr from open clist to delete list
  olist_.del( usr );
  dlist_.add( usr );
}

void tx_svr::teardown_users()
{
  while( !dlist_.empty() ) {
    tx_user *usr = dlist_.first();
    PC_LOG_DBG( "delete_user" ).add("fd", usr->get_fd() ).end();
    usr->close();
    dlist_.del( usr );
    delete usr;
  }
}

void tx_svr::accept( int fd )
{
  // create and add new user
  tx_user *usr = new tx_user;
  usr->set_net_loop( &nl_ );
  usr->set_tx_svr( this );
  usr->set_fd( fd );
  usr->set_block( false );
  if ( usr->init() ) {
    PC_LOG_DBG( "new_user" ).add("fd", fd ).end();
    olist_.add( usr );
  } else {
    usr->close();
    delete usr;
  }
}

void tx_svr::submit( const char *buf, size_t len )
{
  PC_LOG_DBG( "submit tx" )
    .add( "slot", slot_ )
    .add( "num_leaders", avec_.size() )
    .end();
  for( ip_addr& addr: avec_ ) {
    tconn_.send( &addr, buf, len );
  }
}

void tx_svr::add_addr( const ip_addr& addr )
{
  for( ip_addr& iaddr: avec_ ) {
    if ( iaddr == addr ) return;
  }
  avec_.push_back( addr );
}

void tx_svr::on_response( rpc::slot_subscribe *res )
{
  // check error
  if ( PC_UNLIKELY( res->get_is_err() ) ) {
    set_err_msg( "failed to slot_subscribe ["
        + res->get_err_msg()  + "]" );
    return;
  }

  // ignore slots that go back in time
  uint64_t slot = res->get_slot();
  if ( slot <= slot_ ) {
    return;
  }
  slot_ = slot;

  // submit heartbeat every so often to keep connection alive
  if ( slot_cnt_++ % PC_HBEAT_INTERVAL == 0 ) {
    clnt_.send( hreq_ );
  }

  // request next slot leader schedule
  if ( PC_UNLIKELY( lreq_->get_is_recv() &&
                    slot_ > lreq_->get_last_slot() - PC_LEADER_MIN ) ) {
    lreq_->set_slot( slot_ - PC_LEADER_MIN );
    clnt_.send( lreq_ );
  }

  // construct unique list of addresses of the leaders for
  // the previous and next num_slots
  avec_.clear();
  pub_key *pkey = nullptr;
  ip_addr iaddr;
  uint64_t max_slot = std::min( slot_+5, lreq_->get_last_slot() );
  for( uint64_t slot = slot_-1; slot < max_slot; ++slot ) {
    pub_key *ikey = lreq_->get_leader( slot );
    if ( ikey && ( !pkey || *ikey != *pkey) ) {
      if ( creq_->get_ip_addr( *ikey, iaddr ) ) {
        add_addr( iaddr );
      } else {
        PC_LOG_WRN( "missing leader addr" )
          .add( "leader", *ikey )
          .add( "curr_slot", slot )
          .add( "start_slot", slot_-1 )
          .add( "end_slot", slot_+4 )
          .add( "last_slot", lreq_->get_last_slot() )
          .end();
        if ( creq_->get_is_recv() ) {
          clnt_.send( creq_ );
        }
        if ( lreq_->get_is_recv() ) {
          lreq_->set_slot( slot_ - PC_LEADER_MIN );
          clnt_.send( lreq_ );
        }
      }
    }
    pkey = ikey;
  }
  PC_LOG_DBG( "receive slot" )
    .add( "slot", slot_ )
    .add( "num_leaders", avec_.size() )
    .end();
}

void tx_svr::on_response( rpc::get_cluster_nodes *m )
{
  if ( m->get_is_err() ) {
    set_err_msg( "failed to get cluster nodes["
        + m->get_err_msg()  + "]" );
    m->reset_err();
    return;
  }
  PC_LOG_DBG( "received get_cluster_nodes" ).end();
}

void tx_svr::on_response( rpc::get_slot_leaders *m )
{
  if ( m->get_is_err() ) {
    set_err_msg( "failed to get slot leaders ["
        + m->get_err_msg()  + "]" );
    m->reset_err();
    return;
  }
  PC_LOG_DBG( "received get_slot_leaders" )
    .add( "curr_slot", slot_ )
    .add( "last_slot", m->get_last_slot() ).end();
}

void tx_svr::on_response( rpc::get_health *m )
{
  if ( m->get_is_err() ) {
    PC_LOG_WRN( "get_health error" )
      .add( "emsg", m->get_err_msg() )
      .end();
    m->reset_err();
  }
  PC_LOG_DBG( "health update" ).end();
}

void tx_svr::reconnect_rpc()
{
  // check if connection process has complete
  if ( hconn_.get_is_wait() ) {
    hconn_.check();
  }
  if ( wconn_.get_is_wait() ) {
    wconn_.check();
  }
  if ( hconn_.get_is_wait() || wconn_.get_is_wait() ) {
    return;
  }

  // check for successful (re)connect
  if ( !hconn_.get_is_err() && !wconn_.get_is_err() ) {
    PC_LOG_INF( "rpc_connected" ).end();

    // reset state
    has_conn_  = true;
    wait_conn_ = false;
    slot_ = 0L;
    avec_.clear();
    clnt_.reset();
    ctimeout_ = PC_NSECS_IN_SEC;
    lreq_->set_recv_time( 0L ); // in case this request was in-flight

    // subscribe to slots and cluster addresses
    clnt_.send( sreq_ );
    clnt_.send( creq_ );
    return;
  }

  // log disconnect error
  if ( wait_conn_ || has_conn_ ) {
    wait_conn_ = false;
    log_disconnect();
  }

  // wait for reconnect timeout
  has_conn_ = false;
  int64_t ts = get_now();
  if ( ctimeout_ > (ts-cts_) ) {
    return;
  }

  // attempt to reconnect
  cts_ = ts;
  ctimeout_ += ctimeout_;
  ctimeout_ = std::min( ctimeout_, PC_RECONNECT_TIMEOUT );
  wait_conn_ = true;
  hconn_.init();
  wconn_.init();
}

void tx_svr::log_disconnect()
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

void tx_svr::teardown()
{
  PC_LOG_INF( "pyth_tx_svr_teardown" ).end();

  // shutdown listener
  tsvr_.close();

  // destroy any open users
  while( !olist_.empty() ) {
    tx_user *usr = olist_.first();
    olist_.del( usr );
    dlist_.add( usr );
  }
  teardown_users();

  // destroy rpc connections
  hconn_.close();
  wconn_.close();
}
