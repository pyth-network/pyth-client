#include "manager.hpp"
#include "log.hpp"

using namespace pc;

#define PC_RPC_HTTP_PORT      8899
#define PC_RPC_WEBSOCKET_PORT 8900
#define PC_RECONNECT_TIMEOUT  (120L*1000000000L)
#define PC_BLOCKHASH_TIMEOUT  3

///////////////////////////////////////////////////////////////////////////
// manager_sub

manager_sub::~manager_sub()
{
}

void manager_sub::on_connect( manager * )
{
}

void manager_sub::on_disconnect( manager * )
{
}

void manager_sub::on_init( manager * )
{
}

void manager_sub::on_add_symbol( manager *, price * )
{
}

///////////////////////////////////////////////////////////////////////////
// manager

manager::manager()
: sub_( nullptr ),
  status_( 0 ),
  num_sub_( 0 ),
  version_( PC_VERSION ),
  kidx_( 0 ),
  cts_( 0L ),
  ctimeout_( PC_NSECS_IN_SEC ),
  slot_ts_( 0L ),
  slot_int_( 0L ),
  slot_min_( 0L ),
  slot_( 0UL ),
  slot_cnt_( 0UL ),
  wait_conn_( false ),
  do_cap_( false ),
  first_ack_( true )
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
  for( product *ptr: svec_ ) {
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
    // notify user that initialization is complete
    if ( sub_ ) {
      sub_->on_init( this );
    }
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

void manager::set_capture_file( const std::string& cap_file )
{
  cap_.set_file( cap_file );
}

std::string manager::get_capture_file() const
{
  return cap_.get_file();
}

void manager::set_do_capture( bool do_cap )
{
  do_cap_ = do_cap;
}

bool manager::get_do_capture() const
{
  return do_cap_;
}

void manager::set_listen_port( int port )
{
  lsvr_.set_port( port );
}

int manager::get_listen_port() const
{
  return lsvr_.get_port();
}

void manager::set_version( uint32_t version )
{
  version_ = version;
}

uint32_t manager::get_version() const
{
  return version_;
}

void manager::set_content_dir( const std::string& cdir )
{
  cdir_ = cdir;
}

std::string manager::get_content_dir() const
{
  return cdir_;
}

void manager::set_manager_sub( manager_sub *sub )
{
  sub_ = sub;
}

manager_sub *manager::get_manager_sub() const
{
  return sub_;
}

rpc_client *manager::get_rpc_client()
{
  return &clnt_;
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

  // initialize capture
  if ( do_cap_ && !cap_.init() ) {
    return set_err_msg( cap_.get_err_msg() );
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
  hconn_.init();
  wconn_.init();
  wait_conn_ = true;

  // initialize listening port if port defined
  if ( lsvr_.get_port() > 0 ) {
    lsvr_.set_net_accept( this );
    lsvr_.set_net_loop( &nl_ );
    if ( !lsvr_.init() ) {
      return set_err_msg( lsvr_.get_err_msg() );
    }
    PC_LOG_INF("listening").add("port",lsvr_.get_port())
      .add( "content_dir", get_content_dir() )
      .end();
  }
  PC_LOG_INF( "initialized" )
    .add( "version", version_ )
    .add( "capture_file", get_capture_file() )
    .end();

  return true;
}

bool manager::bootstrap()
{
  int status = PC_PYTH_RPC_CONNECTED | PC_PYTH_HAS_BLOCK_HASH;
  if ( get_mapping_pub_key() ) {
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

get_mapping *manager::get_last_mapping() const
{
  return !mvec_.empty() ? mvec_.back() : nullptr;
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

void manager::poll( bool do_wait )
{
  // poll for any socket events
  if ( do_wait ) {
    nl_.poll( 1 );
  } else {
    if ( has_status( PC_PYTH_RPC_CONNECTED ) ) {
      hconn_.poll();
      wconn_.poll();
    }
    if ( lsvr_.get_port() ) {
      lsvr_.poll();
      for( user *uptr = olist_.first(); uptr; ) {
        user *nptr = uptr->get_next();
        uptr->poll();
        uptr = nptr;
      }
    }
  }

  // submit pending requests
  for( request *rptr =plist_.first(); rptr; ) {
    request *nxt = rptr->get_next();
    if ( rptr->get_is_ready() ) {
      plist_.del( rptr );
      rptr->submit();
    }
    rptr = nxt;
  }

  // destroy any users scheduled for deletion
  teardown_users();

  // check if we need to reconnect rpc services
  if ( PC_UNLIKELY( !has_status( PC_PYTH_RPC_CONNECTED ) ||
                    hconn_.get_is_err() || wconn_.get_is_err() ))  {
    reconnect_rpc();
    return;
  }

  // schedule next price update but only if we're connected to rpc port
  int64_t ts = get_now();
  while ( kidx_ < kvec_.size() ) {
    price_sched *kptr = kvec_[kidx_];
    int64_t tot_ts = slot_min_ - ack_ts_ - ack_ts_;
    tot_ts = std::max( slot_min_/10, tot_ts );
    int64_t pub_ts = slot_ts_ + ( tot_ts * kptr->get_hash() ) /
      price_sched::fraction;
    if ( ts > pub_ts ) {
      kptr->schedule();
      ++kidx_;
    } else {
      break;
    }
  }
}

void manager::reconnect_rpc()
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
    set_status( PC_PYTH_RPC_CONNECTED );

    // reset state
    wait_conn_ = false;
    first_ack_ = true;
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
    for( product *ptr: svec_ ) {
      ptr->reset();
      submit( ptr );
      add_map_sub();
      for( unsigned i=0; i != ptr->get_num_price(); ++i ) {
        price *qptr = ptr->get_price( i );
        qptr->reset();
        submit( qptr );
        add_map_sub();
      }
    }
    // callback user with connection status
    if ( sub_ ) {
      sub_->on_connect( this );
    }
    return;
  }

  // log disconnect error
  if ( wait_conn_ || has_status( PC_PYTH_RPC_CONNECTED ) ) {
    wait_conn_ = false;
    log_disconnect();
    if ( sub_ && has_status( PC_PYTH_RPC_CONNECTED ) ) {
      sub_->on_disconnect( this );
    }
  }

  // wait for reconnect timeout
  status_ = 0;
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

  // flush capture
  if ( do_cap_ ) {
    cap_.flush();
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
  static const double afactor = 2./(1+8.);
  int64_t ack_ts = m->get_recv_time() - m->get_sent_time();
  if ( !first_ack_ ) {
    ack_ts_ = (1.-afactor)*ack_ts_ + afactor * ack_ts;
  } else {
    ack_ts_ = ack_ts;
    first_ack_ = false;
  }
  if ( has_status( PC_PYTH_HAS_BLOCK_HASH ) ) {
    return;
  }
  // set initialized status for block hash
  set_status( PC_PYTH_HAS_BLOCK_HASH );
  PC_LOG_INF( "received_recent_block_hash" )
    .add( "slot", m->get_slot() )
    .add( "slot_interval(ms)", 1e-6*slot_int_ )
    .add( "rount_trip_time(ms)", 1e-6*ack_ts_ )
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

void manager::add_product( const pub_key&acc )
{
  acc_map_t::iter_t it = amap_.find( acc );
  if ( !it ) {
    // subscribe to new symbol account
    product *ptr = new product( acc );
    amap_.ref( amap_.add( acc ) ) = ptr;
    svec_.push_back( ptr );
    submit( ptr );
    // add mapping subscription count
    add_map_sub();
  }
}

product *manager::get_product( const pub_key& acc )
{
  acc_map_t::iter_t it = amap_.find( acc );
  return it ? dynamic_cast<product*>( amap_.obj( it ) ) : nullptr;
}

void manager::add_price( const pub_key&acc, product *prod )
{
  // find current symbol account (if any)
  acc_map_t::iter_t it = amap_.find( acc );
  if ( !it ) {
    // subscribe to new symbol account
    price *ptr = new price( acc, prod );
    amap_.ref( amap_.add( acc ) ) = ptr;
    submit( ptr );
    // add price to product
    prod->add_price( ptr );
    // add mapping subscription count
    add_map_sub();
  }
}

price *manager::get_price( const pub_key& acc )
{
  acc_map_t::iter_t it = amap_.find( acc );
  return it ? dynamic_cast<price*>( amap_.obj( it ) ) : nullptr;
}

unsigned manager::get_num_product() const
{
  return svec_.size();
}

product *manager::get_product( unsigned i ) const
{
  return i < svec_.size() ? svec_[i] : nullptr;
}
