#include "request.hpp"
#include "manager.hpp"
#include "log.hpp"
#include <zstd.h>
#include <algorithm>

using namespace pc;

///////////////////////////////////////////////////////////////////////////
// request_sub

request_sub::~request_sub()
{
}

request_node::request_node( request_sub*sptr, request*rptr, uint64_t idx)
: sub_( sptr ), req_( rptr ), idx_( idx ) {
}

request_sub_set::request_sub_set( request_sub *sub )
: sptr_( sub ), sidx_( 0 )
{
}

request_sub_set::~request_sub_set()
{
  teardown();
}

uint64_t request_sub_set::add( request *rptr )
{
  uint32_t sidx;
  if ( !rvec_.empty() ) {
    sidx = rvec_.back();
    rvec_.pop_back();
  } else {
    sidx = sidx_++;
    svec_.resize( sidx_, nullptr );
  }
  request_node *sptr = new request_node(sptr_,rptr,sidx);
  rptr->add_sub( sptr );
  svec_[sidx] = sptr;
  return sidx;
}

bool request_sub_set::del( uint64_t sidx )
{
  if ( sidx >= svec_.size() ) {
    return false;
  }
  request_node *sptr = svec_[sidx];
  if ( !sptr ) {
    return false;
  }
  sptr->req_->del_sub( sptr );
  delete sptr;
  svec_[sidx] = nullptr;
  rvec_.push_back( sidx );
  return true;
}

void request_sub_set::teardown()
{
  for( request_node *sptr: svec_ ) {
    if ( sptr ) {
      sptr->req_->del_sub( sptr );
      delete sptr;
    }
  }
  svec_.clear();
}

///////////////////////////////////////////////////////////////////////////
// request

request::request()
: svr_( nullptr ),
  clnt_( nullptr ),
  is_submit_( false ),
  is_recv_( false )
{
}

void request::set_manager( manager *svr )
{
  svr_ = svr;
}

manager *request::get_manager() const
{
  return svr_;
}

void request::set_rpc_client( rpc_client *clnt )
{
  clnt_ = clnt;
}

rpc_client *request::get_rpc_client() const
{
  return clnt_;
}

bool request::get_is_ready()
{
  return svr_->has_status( PC_PYTH_RPC_CONNECTED |
                           PC_PYTH_HAS_BLOCK_HASH );
}

bool request::get_is_done() const
{
  return true;
}

void request::add_sub( request_node *sptr )
{
  slist_.add( sptr );
}

void request::del_sub( request_node *sptr )
{
  slist_.del( sptr );
}

request::prev_next_t *request::get_request()
{
  return nd_;
}

void request::set_is_submit( bool is_submit )
{
  is_submit_ = is_submit;
}

bool request::get_is_submit() const
{
  return is_submit_;
}

void request::set_is_recv( bool is_recv )
{
  is_recv_ = is_recv;
}

bool request::get_is_recv() const
{
  return is_recv_;
}

void request::on_response( rpc::account_update * )
{
}

///////////////////////////////////////////////////////////////////////////
// get_mapping

get_mapping::get_mapping()
: st_( e_new ),
  num_sym_( 0 )
{
}

void get_mapping::set_mapping_key( const pub_key& mkey )
{
  mkey_ = mkey;
}

pub_key *get_mapping::get_mapping_key()
{
  return &mkey_;
}

uint32_t get_mapping::get_num_symbols() const
{
  return num_sym_;
}

bool get_mapping::get_is_full() const
{
  return num_sym_ >= PC_MAP_TABLE_SIZE;
}

void get_mapping::reset()
{
  reset_err();
  st_ = e_new;
  set_is_recv( false );
}

void get_mapping::submit()
{
  st_ = e_new;
  areq_->set_commitment( get_manager()->get_commitment() );
  areq_->set_account( &mkey_ );
  areq_->set_sub( this );
  // get account data
  get_rpc_client()->send( areq_ );
}

void get_mapping::on_response( rpc::get_account_info *res )
{
  set_is_recv( true );
  update( res );
}

void get_mapping::on_response( rpc::account_update *res )
{
  if ( get_is_recv( )) {
    update( res );
  }
}

template<class T>
void get_mapping::update( T *res )
{
  // check for errors
  manager *cptr = get_manager();
  if ( res->get_is_err() ) {
    cptr->set_err_msg( res->get_err_msg() );
    return;
  }
  pc_map_table_t *tab;
  if ( sizeof( pc_map_table_t ) > res->get_data_ref( tab ) ||
       tab->magic_ != PC_MAGIC ) {
    cptr->set_err_msg( "invalid or corrupt mapping account" );
    return;
  }
  if ( tab->ver_ != PC_VERSION ) {
    cptr->set_err_msg( "invalid mapping account version=" +
        std::to_string( tab->ver_ ) );
  }

  // check and get any new product accounts in mapping table
  num_sym_ = tab->num_;
  PC_LOG_INF( "add_mapping" )
    .add( "account", mkey_ )
    .add( "num_products", num_sym_ )
    .end();
  for( unsigned i=0; i != tab->num_; ++i ) {
    pub_key *aptr = (pub_key*)&tab->prod_[i];
    cptr->add_product( *aptr );
  }

  // subscribe to other mapping accounts
  if ( !pc_pub_key_is_zero( &tab->next_ ) ) {
    cptr->add_mapping( *(pub_key*)&tab->next_ );
  }

  // update state and reduce subscription count after we subscribe to
  // other accounts
  if ( st_ == e_new ) {
    st_ = e_init;
    cptr->del_map_sub();
  }

  // capture mapping attributes to disk
  cptr->write( (pc_pub_key_t*)mkey_.data(), (pc_acc_t*)tab );
}

///////////////////////////////////////////////////////////////////////////
// product

product::product( const pub_key& acc )
: acc_( acc ),
  st_( e_subscribe )
{
  areq_->set_account( &acc_ );
  areq_->set_sub( this );
}

product::~product()
{
  for( price *ptr: pvec_ ) {
    delete ptr;
  }
  pvec_.clear();
}

pub_key *product::get_account()
{
  return &acc_;
}

const pub_key *product::get_account() const
{
  return &acc_;
}

str product::get_symbol()
{
  str sym;
  get_attr( attr_id( "symbol" ), sym);
  return sym;
}

void product::reset()
{
  reset_err();
  st_ = e_subscribe;
  set_is_recv( false );
}

void product::add_price( price *px )
{
  pvec_.push_back( px );
}

void product::submit()
{
  rpc_client  *cptr = get_rpc_client();
  areq_->set_commitment( get_manager()->get_commitment() );
  st_ = e_subscribe;
  cptr->send( areq_ );
}

void product::on_response( rpc::get_account_info *res )
{
  set_is_recv( true );
  update( res );
}

void product::on_response( rpc::account_update *res )
{
  if ( get_is_recv() ) {
    update( res );
  }
}

template<class T>
void product::update( T *res )
{
  // check for errors and deserialize
  manager *cptr = get_manager();
  if ( res->get_is_err() ) {
    cptr->set_err_msg( res->get_err_msg() );
    st_ = e_error;
    return;
  }
  pc_prod_t *prod;
  size_t plen = std::max( sizeof(pc_price_t), (size_t)PC_PROD_ACC_SIZE );
  if ( sizeof( pc_prod_t ) > res->get_data_ref( prod, plen ) ||
       prod->magic_ != PC_MAGIC ||
       !init_from_account( prod ) ) {
    cptr->set_err_msg( "invalid or corrupt product account" );
    st_ = e_error;
    return;
  }
  if ( prod->ver_ != PC_VERSION ) {
    cptr->set_err_msg( "invalid product account version=" +
        std::to_string( prod->ver_ ) );
    st_ = e_error;
    return;
  }

  // subscribe to firstprice account in chain
  if ( !pc_pub_key_is_zero( &prod->px_acc_ ) ) {
    cptr->add_price( *(pub_key*)&prod->px_acc_, this );
  }

  // log product update
  json_wtr wtr;
  wtr.add_val( json_wtr::e_obj );
  write_json( wtr );
  wtr.pop();
  net_buf *jhd, *jtl;
  wtr.detach( jhd, jtl );
  PC_LOG_INF( st_ != e_done ? "add_product" : "upd_product" )
    .add( "account", acc_ )
    .add( "attr", str( jhd->buf_, jhd->size_) )
    .end();
  jhd->dealloc();

  // capture product attributes to disk
  cptr->write( (pc_pub_key_t*)acc_.data(), (pc_acc_t*)prod );

  if ( st_ != e_done ) {
    // log new product
    st_ = e_done;
    // reduce subscription count
    cptr->del_map_sub();
  }

  // ping subscribers with new product details
  on_response_sub( this );
}

bool product::get_is_done() const
{
  return st_ == e_done;
}

unsigned product::get_num_price() const
{
  return pvec_.size();
}

price *product::get_price( unsigned i ) const
{
  return pvec_[i];
}

price *product::get_price( price_type pt ) const
{
  for( price *ptr: pvec_ ) {
    if ( ptr->get_price_type() == pt ) {
      return ptr;
    }
  }
  return nullptr;
}

void product::dump_json( json_wtr& wtr ) const
{
  // assumes the json_wtr has already started an object structure
  wtr.add_key( "account", *get_account() );
  wtr.add_key( "attr_dict", json_wtr::e_obj );
  write_json( wtr );
  wtr.pop();
  wtr.add_key( "price_accounts", json_wtr::e_arr );
  for( unsigned i=0; i != get_num_price(); ++i ) {
    wtr.add_val( json_wtr::e_obj );
    price *ptr = get_price( i );
    ptr->dump_json( wtr );
    wtr.pop();
  }
  wtr.pop();
}

///////////////////////////////////////////////////////////////////////////
// price

price::price( const pub_key& acc, product *prod )
: init_( false ),
  isched_( false ),
  st_( e_subscribe ),
  pub_idx_( (unsigned)-1 ),
  apub_( acc ),
  lamports_( 0UL ),
  pub_slot_( 0UL ),
  prod_( prod ),
  sched_( this ),
  pinit_( this ),
  pptr_(nullptr)
{
  areq_->set_account( &apub_ );
  preq_->set_account( &apub_ );
  areq_->set_sub( this );
  preq_->set_sub( this );
  size_t tlen = ZSTD_compressBound( sizeof(pc_price_t) );
  pptr_ = (pc_price_t*)new char[tlen];
  __builtin_memset( pptr_, 0, tlen );
}

price::~price()
{
  delete [] (char*)pptr_;
  pptr_ = nullptr;
}

bool price::init_publish()
{
  manager *cptr = get_manager();
  key_pair *pkey = cptr->get_publish_key_pair();
  if ( !pkey ) {
    on_error_sub( "missing or invalid publish key [" +
        cptr->get_publish_key_pair_file() + "]", this );
    return false;
  }
  pub_key *gpub = cptr->get_program_pub_key();
  if ( !gpub ) {
    on_error_sub( "missing or invalid program public key [" +
        cptr->get_program_pub_key_file() + "]", this );
    return false;
  }
  preq_->set_publish( pkey );
  preq_->set_pubcache( cptr->get_publish_key_cache() );
  preq_->set_program( gpub );
  init_ = true;
  return true;
}

product *price::get_product() const
{
  return prod_;
}

str price::get_symbol()
{
  return prod_->get_symbol();
}

bool price::get_attr( attr_id aid, str& val ) const
{
  return prod_->get_attr( aid, val );
}

pub_key *price::get_account()
{
  return &apub_;
}

const pub_key *price::get_account() const
{
  return &apub_;
}

uint32_t price::get_version() const
{
  return pptr_->ver_;
}

int64_t  price::get_price() const
{
  return pptr_->agg_.price_;
}

uint64_t price::get_conf() const
{
  return pptr_->agg_.conf_;
}

int64_t price::get_twap() const
{
  return pptr_->twap_.val_;
}

uint64_t price::get_twac() const
{
  return pptr_->twac_.val_;
}

uint64_t price::get_prev_slot() const
{
  return pptr_->prev_slot_;
}

int64_t price::get_prev_price() const
{
  return pptr_->prev_price_;
}

uint64_t price::get_prev_conf() const
{
  return pptr_->prev_conf_;
}

int64_t price::get_price_exponent() const
{
  return pptr_->expo_;
}

price_type price::get_price_type() const
{
  return(price_type) pptr_->ptype_;
}

symbol_status price::get_status() const
{
  return (symbol_status)pptr_->agg_.status_;
}

uint32_t price::get_num_qt() const
{
  return pptr_->num_qt_;
}

uint64_t price::get_lamports() const
{
  return lamports_;
}

uint64_t price::get_valid_slot() const
{
  return pptr_->valid_slot_;
}

uint64_t price::get_pub_slot() const
{
  return pptr_->agg_.pub_slot_;
}

bool price::get_is_ready_publish() const
{
  if ( st_ != e_publish )
    return false;
  manager *cptr = get_manager();
  if ( cptr->get_do_tx() ) {
    return cptr->get_is_tx_connect();
  } else {
    return cptr->has_status( PC_PYTH_RPC_CONNECTED | PC_PYTH_HAS_BLOCK_HASH );
  }
}

void price::reset()
{
  st_ = e_subscribe;
  reset_err();
  set_is_recv( false );
}

bool price::has_publisher()
{
  return pub_idx_ != (unsigned)-1;
}

bool price::has_publisher( const pub_key& key )
{
  pc_pub_key_t *pk = (pc_pub_key_t*)key.data();
  for(uint32_t i=0; i != pptr_->num_; ++i ) {
    pc_pub_key_t *ikey = &pptr_->comp_[i].pub_;
    if ( pc_pub_key_equal( ikey, pk ) ) {
      return true;
    }
  }
  return false;
}

price_sched *price::get_sched()
{
  if ( !isched_ ) {
    isched_ = true;
    get_manager()->submit( &sched_ );
  }
  return &sched_;
}

bool price::update()
{
  return update( 0L, 0UL, symbol_status::e_unknown, true );
}

bool price::update(
    int64_t price, uint64_t conf, symbol_status st )
{
  return update( price, conf, st, false );
}

bool price::update(
    int64_t price, uint64_t conf, symbol_status st, bool is_agg )
{
  if ( PC_UNLIKELY( !init_ && !init_publish() ) ) {
    return false;
  }
  if ( PC_UNLIKELY( !has_publisher() ) ) {
    return false;
  }
  if ( PC_UNLIKELY( !get_is_ready_publish() ) ) {
    return false;
  }
  manager *mgr = get_manager();
  const uint64_t slot = mgr->get_slot();
  preq_->set_price( price, conf, st, slot, is_agg );
  preq_->set_block_hash( mgr->get_recent_block_hash() );
  if ( mgr->get_do_tx() )
    mgr->submit( preq_ );
  else {
    get_rpc_client()->send( preq_ );
    tvec_.emplace_back( std::string( 100, '\0' ), preq_->get_sent_time() );
    preq_->get_signature()->enc_base58( tvec_.back().first );
    PC_LOG_DBG( "sent price update transaction" )
      .add( "price_account", *get_account() )
      .add( "product_account", *prod_->get_account() )
      .add( "symbol", get_symbol() )
      .add( "price_type", price_type_to_str( get_price_type() ) )
      .add( "sig", tvec_.back().first )
      .add( "pub_slot", slot )
      .end();
    if ( PC_UNLIKELY( tvec_.size() >= 100 ) ) {
      PC_LOG_WRN( "too many unacked price update transactions" )
        .add( "price_account", *get_account() )
        .add( "product_account", *prod_->get_account() )
        .add( "symbol", get_symbol() )
        .add( "price_type", price_type_to_str( get_price_type() ) )
        .add( "num_txid", tvec_.size() )
        .end();
      tvec_.erase( tvec_.begin(), tvec_.begin() + 50 );
    }
  }
  inc_sent();
  return true;
}

void price::submit()
{
  if ( st_ == e_subscribe ) {
    // subscribe first
    areq_->set_commitment( get_manager()->get_commitment() );
    rpc_client  *cptr = get_rpc_client();
    cptr->send( areq_ );
    st_ = e_sent_subscribe;
  }
}

bool price::has_unacked_updates() const
{
  return ! tvec_.empty();
}

void price::on_response( rpc::upd_price *res )
{
  std::string txid = res->get_ack_signature().as_string();
  const auto it = std::find_if( tvec_.begin(), tvec_.end(),
      [&] ( const std::pair<std::string,int64_t>& m ) { return m.first == txid; } );
  if ( it == tvec_.end() )
    return;
  const int64_t ack_dur = res->get_recv_time() - it->second;
  tvec_.erase( it );
  PC_LOG_DBG( "received price update transaction ack" )
    .add( "price_account", *get_account() )
    .add( "product_account", *prod_->get_account() )
    .add( "symbol", get_symbol() )
    .add( "price_type", price_type_to_str( get_price_type() ) )
    .add( "sig", txid )
    .add( "round_trip_time(ms)", 1e-6 * ack_dur )
    .end();
}

void price::on_response( rpc::get_account_info *res )
{
  set_is_recv( true );
  update( res );
}

void price::on_response( rpc::account_update *res )
{
  if ( get_is_recv() ) {
    update( res );
  }
}

void price::log_update( const char *title )
{
  PC_LOG_INF( title )
    .add( "account", *get_account() )
    .add( "product", *prod_->get_account() )
    .add( "symbol", get_symbol() )
    .add( "price_type", price_type_to_str( get_price_type() ) )
    .add( "version", get_version() )
    .add( "exponent", get_price_exponent() )
    .add( "num_publishers", get_num_publisher() )
    .end();
}

void price::unsubscribe()
{
  set_is_recv( false );
}

void price::init_subscribe()
{
  // switch state
  st_ = e_publish;

  // update publisher index
  update_pub();

  // subscribe to next symbol in chain
  manager *mgr = get_manager();
  if ( !pc_pub_key_is_zero( &pptr_->next_ ) ) {
    mgr->add_price( *(pub_key*)&pptr_->next_, prod_ );
  }

  // log new price object
  log_update( "add_price" );

  // callback users on new symbol update
  manager_sub *sub = mgr->get_manager_sub();
  if ( sub ) {
    sub->on_add_symbol( mgr, this );
  }

  // reduce subscription count after we subscribe to next symbol in chain
  // do this last in case this triggers an init callback
  mgr->del_map_sub();
}

template<class T>
void price::update( T *res )
{
  // check for errors
  if ( res->get_is_err() ) {
    on_error_sub( res->get_err_msg(), this );
    st_ = e_error;
    return;
  }

  // get account data
  size_t tlen = ZSTD_compressBound( sizeof(pc_price_t) );
  res->get_data_val( pptr_, tlen );
  if ( PC_UNLIKELY( pptr_->magic_ != PC_MAGIC ) ) {
    on_error_sub( "bad price account header", this );
    st_ = e_error;
    return;
  }

  // price account was (re) initialized
  if ( PC_UNLIKELY( pptr_->agg_.pub_slot_ == 0L ) ) {
    log_update( "init_price" );
    on_response_sub( &pinit_ );
  }

  // update state and subscribe to next price account in the chain
  if ( PC_UNLIKELY( st_ == e_sent_subscribe ) ) {
    init_subscribe();
  }

  // update publishers
  update_pub();
  lamports_ = res->get_lamports();
  manager *mgr = get_manager();

  // update aggregate price and status if changed
  if ( pub_slot_ != pptr_->agg_.pub_slot_ || pub_slot_ == 0UL ) {
    // subscription service dropped an update
    if ( pub_slot_ < pptr_->valid_slot_ && pub_slot_ != 0UL ) {
      inc_sub_drop();
    }
    pub_slot_ = pptr_->agg_.pub_slot_;

    // capture aggregate price and components to disk
    mgr->write( (pc_pub_key_t*)apub_.data(), (pc_acc_t*)pptr_ );

    // add slot/time latency statistics
    if ( pub_idx_ != (unsigned)-1 ) {
      uint64_t pub_slot = pptr_->comp_[pub_idx_].agg_.pub_slot_;
      add_recv( mgr->get_slot(), pub_slot_, pub_slot );
    }

    // ping subscribers with new aggregate price
    on_response_sub( this );
  }
}

void price::update_pub()
{
  // update publishing index
  pub_idx_ = (unsigned)-1;
  pub_key *pkey = get_manager()->get_publish_pub_key();
  if ( pkey ) {
    for( unsigned i=0; i != pptr_->num_; ++i ) {
      if ( pc_pub_key_equal( &pptr_->comp_[i].pub_, (pc_pub_key_t*)pkey ) ) {
        pub_idx_ = i;
        break;
      }
    }
  }
}

bool price::get_is_done() const
{
  return st_ == e_publish;
}

unsigned price::get_num_publisher() const
{
  return pptr_->num_;
}

const pub_key *price::get_publisher( unsigned i ) const
{
  return (const pub_key*)&pptr_->comp_[i].pub_;
}

int64_t price::get_publisher_price( unsigned i ) const
{
  return pptr_->comp_[i].agg_.price_;
}

uint64_t price::get_publisher_conf( unsigned i ) const
{
  return pptr_->comp_[i].agg_.conf_;
}

uint64_t price::get_publisher_slot( unsigned i ) const
{
  return pptr_->comp_[i].agg_.pub_slot_;
}

symbol_status price::get_publisher_status( unsigned i ) const
{
  return (symbol_status)pptr_->comp_[i].agg_.status_;
}

void price::dump_json( json_wtr& wtr ) const
{
  // assumes the json_wtr has already started an object structure
  wtr.add_key( "account", *get_account() );
  wtr.add_key( "price_type", price_type_to_str( get_price_type() ));
  wtr.add_key( "price_exponent", get_price_exponent() );
  wtr.add_key( "status", symbol_status_to_str( get_status() ) );
  wtr.add_key( "price", get_price() );
  wtr.add_key( "conf", get_conf() );
  wtr.add_key( "twap", get_twap() );
  wtr.add_key( "twac", get_twac() );
  wtr.add_key( "valid_slot", get_valid_slot() );
  wtr.add_key( "pub_slot", get_pub_slot() );
  wtr.add_key( "prev_slot", get_prev_slot() );
  wtr.add_key( "prev_price", get_prev_price() );
  wtr.add_key( "prev_conf", get_prev_conf() );
  wtr.add_key( "publisher_accounts", json_wtr::e_arr );
  for( unsigned i=0; i != get_num_publisher(); ++i ) {
    wtr.add_val( json_wtr::e_obj );
    wtr.add_key( "account", *get_publisher( i ) );
    wtr.add_key( "status", symbol_status_to_str(
          get_publisher_status(i) ) );
    wtr.add_key( "price", get_publisher_price(i) );
    wtr.add_key( "conf", get_publisher_conf(i) );
    wtr.add_key( "slot", get_publisher_slot(i) );
    wtr.pop();
  }
  wtr.pop();
}

///////////////////////////////////////////////////////////////////////////
// price_sched

price_sched::price_sched( price *ptr )
: ptr_( ptr ),
  shash_( 0UL )
{
}

price *price_sched::get_price() const
{
  return ptr_;
}

uint64_t price_sched::get_hash() const
{
  return shash_;
}

bool price_sched::get_is_ready()
{
  manager *cptr = get_manager();
  return cptr->has_status( PC_PYTH_RPC_CONNECTED |
                           PC_PYTH_HAS_BLOCK_HASH |
                           PC_PYTH_HAS_MAPPING );
}

void price_sched::submit()
{
  uint64_t *iptr = (uint64_t*)ptr_->get_account()->data();
  shash_ = iptr[0]^iptr[2];
  shash_ = shash_ % fraction;
  get_manager()->schedule( this );
}

void price_sched::schedule()
{
  on_response_sub( this );
}

///////////////////////////////////////////////////////////////////////////
// price_init

price_init::price_init( price *ptr )
: ptr_( ptr )
{
}

price *price_init::get_price() const
{
  return ptr_;
}

void price_init::submit()
{
}
