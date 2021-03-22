#include "manager.hpp"
#include "log.hpp"

using namespace pc;

#define PC_RPC_HTTP_PORT      8899
#define PC_RPC_WEBSOCKET_PORT 8900
#define PC_RECONNECT_TIMEOUT  (120L*1000000000L)
#define PC_BLOCKHASH_TIMEOUT  5

///////////////////////////////////////////////////////////////////////////
// manager

manager::manager()
: status_( 0 ),
  num_sub_( 0 ),
  version_( PC_VERSION ),
  kidx_( 0 ),
  cts_( 0L ),
  ctimeout_( PC_NSECS_IN_SEC ),
  slot_ts_( 0L ),
  slot_int_( 0L ),
  slot_min_( 0L ),
  slot_( 0UL ),
  slot_cnt_( 0UL )
{
  breq_->set_sub( this );
  sreq_->set_sub( this );
}

manager::~manager()
{
  for( get_mapping *mptr: mvec_ ) {
    delete mptr;
  }
  mvec_.clear();
  for( price *ptr: svec_ ) {
    delete ptr;
  }
  svec_.clear();
  teardown();
}

void manager::add_map_sub()
{
  num_sub_++;
}

void manager::del_map_sub()
{
  if ( --num_sub_ <= 0 && !has_status( PC_PYTH_HAS_MAPPING ) ) {
    set_status( PC_PYTH_HAS_MAPPING );
    PC_LOG_INF( "completed_mapping_init" ).end();
  }
}

void manager::set_rpc_host( const std::string& rhost )
{
  rhost_ = rhost;
}

std::string manager::get_rpc_host() const
{
  return rhost_;
}

void manager::set_listen_port( int port )
{
  lsvr_.set_port( port );
}

int manager::get_listen_port() const
{
  return lsvr_.get_port();
}

hash *manager::get_recent_block_hash()
{
  return breq_->get_block_hash();
}

uint64_t manager::get_slot() const
{
  return slot_;
}

int64_t manager::get_slot_time() const
{
  return slot_ts_;
}

int64_t manager::get_slot_interval() const
{
  return slot_int_;
}

void manager::teardown()
{
  PC_LOG_INF( "pythd_teardown" ).end();

  // shutdown listener
  lsvr_.close();

  // destroy any open users
  while( !olist_.empty() ) {
    user *usr = olist_.first();
    olist_.del( usr );
    dlist_.add( usr );
  }
  teardown_users();

  // destroy rpc connections
  hconn_.close();
  wconn_.close();
}

bool manager::init()
{
  // read key directory
  if ( !key_store::init() ) {
    return false;
  }

  // log import key names
  key_pair *kp = get_publish_key_pair();
  if ( kp ) {
    PC_LOG_INF( "publish_key" ).add( "key_name", *kp ).end();
  }
  pub_key *mpub = get_mapping_pub_key();
  if ( mpub ) {
    PC_LOG_INF( "mapping_key" ).add( "key_name", *mpub ).end();
  }
  pub_key *gpub = get_program_pub_key();
  if ( gpub ) {
    PC_LOG_INF( "program_key" ).add( "key_name", *gpub ).end();
  }

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
    // submit slot subscription
    clnt_.send( sreq_ );
  }

  // initialize listening port if port defined
  if ( lsvr_.get_port() > 0 ) {
    lsvr_.set_net_accept( this );
    lsvr_.set_net_loop( &nl_ );
    if ( !lsvr_.init() ) {
      return set_err_msg( lsvr_.get_err_msg() );
    }
    PC_LOG_INF("listening").add("port",lsvr_.get_port()).end();
  }

  // wait for bootstrap
  int status = PC_PYTH_RPC_CONNECTED | PC_PYTH_HAS_BLOCK_HASH;
  if ( mpub ) {
    status |= PC_PYTH_HAS_MAPPING;
  }
  while( !get_is_err() && !has_status( status ) ) {
    poll();
  }
  return !get_is_err();
}

void manager::add_mapping( const pub_key& acc )
{
  // check if he have this mapping
  for( get_mapping *mptr: mvec_ ) {
    if ( *mptr->get_mapping_key() == acc ) {
      return;
    }
  }

  // construct and submit mapping account subscription
  get_mapping *mptr = new get_mapping;
  mptr->set_mapping_key( acc );
  mvec_.push_back( mptr );
  submit( mptr );

  // add mapping subscription count
  add_map_sub();
}

bool manager::has_status( int status ) const
{
  return status == (status_ & status);
}

void manager::set_status( int status )
{
  status_ |= status;
}

void manager::reset_status( int status )
{
  status_ &= ~status;
}

void manager::poll()
{
  // poll for any socket events
  nl_.poll( 1 );

  // submit pending requests
  for( request *rptr =plist_.first(); rptr; ) {
    request *nxt = rptr->get_next();
    if ( rptr->get_is_ready() ) {
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

  // schedule next price update
  int64_t ts = get_now();
  while ( kidx_ < kvec_.size() ) {
    price_sched *kptr = kvec_[kidx_];
    int64_t pub_ts = slot_ts_ + ( slot_min_ * kptr->get_hash() ) /
      price_sched::period;
    if ( ts > pub_ts ) {
      kptr->schedule();
      ++kidx_;
    } else {
      break;
    }
  }

  // destroy any users scheduled for deletion
  teardown_users();
}

void manager::reconnect_rpc()
{
  // log disconnect error
  if ( has_status( PC_PYTH_RPC_CONNECTED ) ) {
    log_disconnect();
  }
  status_ = 0;
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

    // reset state
    ctimeout_ = PC_NSECS_IN_SEC;
    slot_ts_ = slot_int_ = 0L;
    slot_cnt_ = 0UL;
    num_sub_ = 0;
    clnt_.reset();
    plist_.clear();

    // resubmit slot subscription
    clnt_.send( sreq_ );

    // resubscribe to mapping and symbol accounts
    for( get_mapping *mptr: mvec_ ) {
      mptr->reset();
      submit( mptr );
      add_map_sub();
    }
    for( price *ptr: svec_ ) {
      ptr->reset();
      submit( ptr );
      add_map_sub();
    }
  } else {
    log_disconnect();
  }
}

void manager::log_disconnect()
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

void manager::teardown_users()
{
  while( !dlist_.empty() ) {
    user *usr = dlist_.first();
    PC_LOG_DBG( "delete_user" ).add("fd", usr->get_fd() ).end();
    usr->close();
    dlist_.del( usr );
  }
}

void manager::accept( int fd )
{
  // create and add new user
  user *usr = new user;
  usr->set_rpc_client( &clnt_ );
  usr->set_net_loop( &nl_ );
  usr->set_manager( this );
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

void manager::del_user( user *usr )
{
  // move usr from open clist to delete list
  olist_.del( usr );
  dlist_.add( usr );
}

void manager::schedule( price_sched *kptr )
{
  kvec_.push_back( kptr );
  for( unsigned i = kvec_.size()-1; i; --i ) {
    if ( kvec_[i]->get_hash() < kvec_[i-1]->get_hash() ) {
      std::swap( kvec_[i], kvec_[i-1] );
    }
  }
}

void manager::on_response( rpc::slot_subscribe *res )
{
  if ( !slot_ts_ ) {
    slot_ts_ = res->get_recv_time();
    return;
  }
  slot_ = res->get_slot();
  slot_int_ = res->get_recv_time() - slot_ts_;
  slot_ts_ = res->get_recv_time();
  if ( slot_cnt_++ % PC_BLOCKHASH_TIMEOUT == 0 ) {
    // submit block hash every N slots
    clnt_.send( breq_ );
  }
  // reset scheduler
  kidx_ = 0;

  // derive minimum slot interval
  if ( slot_min_ ) {
    slot_min_ = std::min( slot_int_, slot_min_ );
  } else {
    slot_min_ = slot_int_;
  }
  PC_LOG_DBG( "receive slot" )
   .add( "slot_num", res->get_slot() )
   .add( "slot_int", 1e-6*slot_int_ )
   .add( "slot_min", 1e-6*slot_min_ )
   .end();
}

void manager::on_response( rpc::get_recent_block_hash *m )
{
  if ( m->get_is_err() ) {
    set_err_msg( "failed to get recent block hash ["
        + m->get_err_msg()  + "]" );
    return;
  }
  if ( has_status( PC_PYTH_HAS_BLOCK_HASH ) ) {
    return;
  }
  // set initialized status for block hash
  set_status( PC_PYTH_HAS_BLOCK_HASH );
  PC_LOG_INF( "received_recent_block_hash" )
    .add( "slot", m->get_slot() )
    .add( "slot_interval(ms)", 1e-6*slot_int_ )
    .end();

  // subscribe to mapping account if not done before
  pub_key *mpub = get_mapping_pub_key();
  if ( mvec_.empty() && mpub ) {
    add_mapping( *mpub );
  }
}

void manager::submit( request *req )
{
  req->set_manager( this );
  req->set_rpc_client( &clnt_ );
  plist_.add( req );
}

void manager::add_symbol( const pub_key&acc )
{
  // find current symbol account (if any)
  acc_map_t::iter_t it = amap_.find( acc );
  if ( !it ) {
    // subscribe to new symbol account
    price *ptr = new price( acc );
    amap_.ref( amap_.add( acc ) ) = ptr;
    svec_.push_back( ptr );
    submit( ptr );
    // add mapping subscription count
    add_map_sub();
  }
}

void manager::add_symbol(
    const symbol& sym, price_type ptype, price *ptr )
{
  symbol_key sk;
  sk.sym_   = sym;
  sk.ptype_ = ptype;
  sym_map_t::iter_t it = smap_.find( sk );
  if ( it ) {
    // replace with newer version account but only if the version
    // is less than we're currently running
    price *prv = smap_.obj( it );
    if ( ptr->get_version() > prv->get_version() &&
         ptr->get_version() <= version_ ) {
      smap_.ref( it ) = ptr;
      // TODO: add prv to unsubscribe and demolition list
    } else {
      // TODO: add ptr to unsubscribe and demolition list
    }
  } else {
    smap_.ref( smap_.add( sk ) ) = ptr;
  }
}

price *manager::get_symbol( const symbol& sym, price_type ptype )
{
  symbol_key sk;
  sk.sym_   = sym;
  sk.ptype_ = ptype;
  sym_map_t::iter_t it = smap_.find( sk );
  if ( it ) {
    return smap_.obj( it );
  } else {
    return nullptr;
  }
}

unsigned manager::get_num_symbol() const
{
  return svec_.size();
}

price *manager::get_symbol( unsigned i ) const
{
  return i < svec_.size() ? svec_[i] : nullptr;
}
