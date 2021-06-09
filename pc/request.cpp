#include "request.hpp"
#include "manager.hpp"
#include "mem_map.hpp"
#include "log.hpp"
#include <algorithm>
#include <math.h>
#include <iostream>

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
  cb_( nullptr )
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

///////////////////////////////////////////////////////////////////////////
// init_mapping

init_mapping::init_mapping()
: st_( e_create_sent ),
  cmt_( commitment::e_confirmed )
{
}

void init_mapping::set_commitment( commitment cmt )
{
  cmt_= cmt;
}

void init_mapping::set_lamports( uint64_t lamports )
{
  creq_->set_lamports( lamports );
}

void init_mapping::submit()
{
  // get keys
  manager *sptr = get_manager();
  key_pair *pkey = sptr->get_publish_key_pair();
  if ( !pkey ) {
    on_error_sub( "missing or invalid publish key [" +
        sptr->get_publish_key_pair_file(), this );
    return;
  }
  key_pair *mkey = sptr->get_mapping_key_pair();
  if ( mkey ) {
    on_error_sub( "mapping key pair already exists [" +
        sptr->get_mapping_key_pair_file() + "]", this );
    return;
  }
  mkey = sptr->create_mapping_key_pair();
  if ( !mkey ) {
    on_error_sub( "failed to create mapping key pair [" +
        sptr->get_mapping_key_pair_file() + "]", this );
    return;
  }
  pub_key *gpub = sptr->get_program_pub_key();
  if ( !gpub ) {
    on_error_sub( "missing or invalid program public key [" +
        sptr->get_program_pub_key_file() + "]", this );
    return;
  }
  creq_->set_sub( this );
  creq_->set_sender( pkey );
  creq_->set_account( mkey );
  creq_->set_owner( gpub );
  creq_->set_space( sizeof( pc_map_table_t ) );
  ireq_->set_sub( this );
  ireq_->set_publish( pkey );
  ireq_->set_mapping( mkey );
  ireq_->set_program( gpub );
  sig_->set_sub( this );

  // get recent block hash and submit request
  st_ = e_create_sent;
  creq_->set_block_hash( get_manager()->get_recent_block_hash() );
  get_rpc_client()->send( creq_ );
}

void init_mapping::on_response( rpc::create_account *res )
{
  if ( res->get_is_err() ) {
    on_error_sub( res->get_err_msg(), this );
    st_ = e_error;
  } else if ( st_ == e_create_sent ) {
    // subscribe to signature completion
    st_ = e_create_sig;
    sig_->set_commitment( commitment::e_finalized );
    sig_->set_signature( res->get_signature() );
    get_rpc_client()->send( sig_ );
  }
}

void init_mapping::on_response( rpc::init_mapping *res )
{
  if ( res->get_is_err() ) {
    on_error_sub( res->get_err_msg(), this );
    st_ = e_error;
  } else if ( st_ == e_init_sent ) {
    // subscribe to signature completion
    st_ = e_init_sig;
    sig_->set_commitment( cmt_ );
    sig_->set_signature( res->get_signature() );
    get_rpc_client()->send( sig_ );
  }
}

void init_mapping::on_response( rpc::signature_subscribe *res )
{
  if ( res->get_is_err() ) {
    on_error_sub( res->get_err_msg(), this );
    st_ = e_error;
  } else if ( st_ == e_create_sig ) {
    st_ = e_init_sent;
    ireq_->set_block_hash( get_manager()->get_recent_block_hash() );
    get_rpc_client()->send( ireq_ );
  } else if ( st_ == e_init_sig ) {
    st_ = e_done;
    on_response_sub( this );
  }
}

bool init_mapping::get_is_done() const
{
  return st_ == e_done;
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
}

void get_mapping::submit()
{
  st_ = e_new;
  areq_->set_account( &mkey_ );
  sreq_->set_account( &mkey_ );
  areq_->set_sub( this );
  sreq_->set_sub( this );
  // submit subscription first
  get_rpc_client()->send( sreq_ );
  get_rpc_client()->send( areq_ );
}

void get_mapping::on_response( rpc::get_account_info *res )
{
  update( res );
}

void get_mapping::on_response( rpc::account_subscribe *res )
{
  update( res );
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
  if ( sizeof( pc_map_table_t ) > res->get_data( tab ) ||
       tab->magic_ != PC_MAGIC ) {
    cptr->set_err_msg( "invalid or corrupt mapping account" );
    return;
  }
  if ( tab->ver_ != PC_VERSION ) {
    cptr->set_err_msg( "invalid mapping account version=" +
        std::to_string( tab->ver_ ) );
  }

  // check and subscribe to any new price accounts in mapping table
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
// add_mapping

add_mapping::add_mapping()
: st_( e_create_sent ),
  cmt_( commitment::e_confirmed )
{
}

void add_mapping::set_commitment( commitment cmt )
{
  cmt_= cmt;
}

void add_mapping::set_lamports( uint64_t lamports )
{
  areq_->set_lamports( lamports );
}

bool add_mapping::get_is_ready()
{
  manager *cptr = get_manager();
  if ( !cptr->get_mapping_pub_key() ) {
    return set_err_msg( "missing mapping key pairfile ["
        + cptr->get_mapping_key_pair_file() + "]" );
  }
  return cptr->has_status( PC_PYTH_RPC_CONNECTED |
                           PC_PYTH_HAS_BLOCK_HASH |
                           PC_PYTH_HAS_MAPPING );
}

void add_mapping::submit()
{
  // get keys
  manager *cptr = get_manager();
  key_pair *pkey = cptr->get_publish_key_pair();
  if ( !pkey ) {
    on_error_sub( "missing or invalid publish key [" +
        cptr->get_publish_key_pair_file(), this );
    return;
  }
  key_pair *mkey = cptr->get_mapping_key_pair();
  if ( !mkey ) {
    on_error_sub( "missing or invalid mapping key [" +
        cptr->get_mapping_key_pair_file() + "]", this );
    return;
  }
  get_mapping *mptr = cptr->get_last_mapping();
  if ( !mptr->get_is_full() ) {
    on_error_sub( "last mapping table not yet full", this );
    return;
  }
  pub_key *mpub = mptr->get_mapping_key();
  if ( mpub[0] == *cptr->get_mapping_pub_key() ) {
    mreq_->set_mapping( mkey );
  } else if ( cptr->get_account_key_pair( *mpub, mkey_ ) ) {
    mreq_->set_mapping( &mkey_ );
  } else {
    on_error_sub( "cant find last mapping key_pair", this );
    return;
  }
  pub_key *gpub = cptr->get_program_pub_key();
  if ( !gpub ) {
    on_error_sub( "missing or invalid program public key [" +
        cptr->get_program_pub_key_file() + "]", this );
    return;
  }
  if ( !cptr->create_account_key_pair( akey_ ) ) {
    on_error_sub( "failed to create new mapping key_pair", this );
    return;
  }
  areq_->set_sender( pkey );
  areq_->set_account( &akey_ );
  areq_->set_owner( gpub );
  areq_->set_space( sizeof( pc_map_table_t ) );
  mreq_->set_program( gpub );
  mreq_->set_publish( pkey );
  mreq_->set_account( &akey_ );
  areq_->set_sub( this );
  mreq_->set_sub( this );
  sig_->set_sub( this );

  // get recent block hash and submit request
  st_ = e_create_sent;
  areq_->set_block_hash( get_manager()->get_recent_block_hash() );
  get_rpc_client()->send( areq_ );
}

void add_mapping::on_response( rpc::create_account *res )
{
  if ( res->get_is_err() ) {
    on_error_sub( res->get_err_msg(), this );
    st_ = e_error;
  } else if ( st_ == e_create_sent ) {
    // subscribe to signature completion
    st_ = e_create_sig;
    sig_->set_commitment( commitment::e_finalized );
    sig_->set_signature( res->get_signature() );
    get_rpc_client()->send( sig_ );
  }
}

void add_mapping::on_response( rpc::add_mapping *res )
{
  if ( res->get_is_err() ) {
    on_error_sub( res->get_err_msg(), this );
    st_ = e_error;
  } else if ( st_ == e_init_sent ) {
    // subscribe to signature completion
    st_ = e_init_sig;
    sig_->set_commitment( cmt_ );
    sig_->set_signature( res->get_signature() );
    get_rpc_client()->send( sig_ );
  }
}

void add_mapping::on_response( rpc::signature_subscribe *res )
{
  if ( res->get_is_err() ) {
    on_error_sub( res->get_err_msg(), this );
    st_ = e_error;
  } else if ( st_ == e_create_sig ) {
    st_ = e_init_sent;
    mreq_->set_block_hash( get_manager()->get_recent_block_hash() );
    get_rpc_client()->send( mreq_ );
  } else if ( st_ == e_init_sig ) {
    st_ = e_done;
    on_response_sub( this );
  }
}

bool add_mapping::get_is_done() const
{
  return st_ == e_done;
}

///////////////////////////////////////////////////////////////////////////
// add_product

add_product::add_product()
: st_( e_create_sent ),
  cmt_( commitment::e_confirmed )
{
}

void add_product::set_lamports( uint64_t funds )
{
  areq_->set_lamports( funds );
}

void add_product::set_commitment( commitment cmt )
{
  cmt_ = cmt;
}

key_pair *add_product::get_account()
{
  return &akey_;
}

bool add_product::get_is_ready()
{
  manager *cptr = get_manager();
  if ( !cptr->get_mapping_pub_key() ) {
    return set_err_msg( "missing mapping key pairfile ["
        + cptr->get_mapping_key_pair_file() + "]" );
  }
  return cptr->has_status( PC_PYTH_RPC_CONNECTED |
                           PC_PYTH_HAS_BLOCK_HASH |
                           PC_PYTH_HAS_MAPPING );
}

void add_product::submit()
{
  manager *cptr = get_manager();
  key_pair *mkey = cptr->get_mapping_key_pair();
  if ( !mkey ) {
    on_error_sub( "missing or invalid mapping key [" +
        cptr->get_mapping_key_pair_file() + "]", this );
    return;
  }
  get_mapping *mptr = cptr->get_last_mapping();
  if ( mptr->get_is_full() ) {
    on_error_sub( "mapping table full. run add_mapping first", this );
    return;
  }
  pub_key *mpub = mptr->get_mapping_key();
  if ( mpub[0] == *cptr->get_mapping_pub_key() ) {
    sreq_->set_mapping( mkey );
  } else if ( cptr->get_account_key_pair( *mpub, mkey_ ) ) {
    sreq_->set_mapping( &mkey_ );
  } else {
    on_error_sub( "cant find last mapping key_pair", this );
    return;
  }
  key_pair *pkey = cptr->get_publish_key_pair();
  if ( !pkey ) {
    on_error_sub( "missing or invalid publish key [" +
        cptr->get_publish_key_pair_file() + "]", this );
    return;
  }
  pub_key *gpub = cptr->get_program_pub_key();
  if ( !gpub ) {
    on_error_sub( "missing or invalid program public key [" +
        cptr->get_program_pub_key_file() + "]", this );
    return;
  }
  if ( !cptr->create_account_key_pair( akey_ ) ) {
    on_error_sub( "failed to create new symbol key_pair", this );
    return;
  }
  areq_->set_sender( pkey );
  areq_->set_account( &akey_ );
  areq_->set_owner( gpub );
  areq_->set_space( sizeof( pc_price_t ) );
  sreq_->set_program( gpub );
  sreq_->set_publish( pkey );
  sreq_->set_account( &akey_ );
  areq_->set_sub( this );
  sreq_->set_sub( this );
  sig_->set_sub( this );

  // get recent block hash and submit request
  st_ = e_create_sent;
  areq_->set_block_hash( get_manager()->get_recent_block_hash() );
  get_rpc_client()->send( areq_ );
}

void add_product::on_response( rpc::create_account *res )
{
  if ( res->get_is_err() ) {
    on_error_sub( res->get_err_msg(), this );
    st_ = e_error;
  } else if ( st_ == e_create_sent ) {
    // subscribe to signature completion
    st_ = e_create_sig;
    sig_->set_commitment( commitment::e_finalized );
    sig_->set_signature( res->get_signature() );
    get_rpc_client()->send( sig_ );
  }
}

void add_product::on_response( rpc::signature_subscribe *res )
{
  if ( res->get_is_err() ) {
    on_error_sub( res->get_err_msg(), this );
    st_ = e_error;
  } else if ( st_ == e_create_sig ) {
    st_ = e_add_sent;
    sreq_->set_block_hash( get_manager()->get_recent_block_hash() );
    get_rpc_client()->send( sreq_ );
  } else if ( st_ == e_add_sig ) {
    st_ = e_done;
    on_response_sub( this );
  }
}

void add_product::on_response( rpc::add_product *res )
{
  if ( res->get_is_err() ) {
    on_error_sub( res->get_err_msg(), this );
    st_ = e_error;
  } else if ( st_ == e_add_sent ) {
    st_ = e_add_sig;
    sig_->set_commitment( cmt_ );
    sig_->set_signature( res->get_signature() );
    get_rpc_client()->send( sig_ );
  }
}

bool add_product::get_is_done() const
{
  return st_ == e_done;
}

///////////////////////////////////////////////////////////////////////////
// upd_product

upd_product::upd_product()
: st_( e_sent ),
  cmt_( commitment::e_confirmed )
{
}

void upd_product::set_commitment( commitment cmt )
{
  cmt_ = cmt;
}

void upd_product::set_product( product *prod )
{
  prod_ = prod;
}

void upd_product::set_attr_dict( attr_dict *aptr )
{
  sreq_->set_attr_dict( aptr );
}

void upd_product::reset()
{
  reset_err();
  st_ = e_sent;
}

bool upd_product::get_is_ready()
{
  manager *cptr = get_manager();
  if ( !cptr->get_mapping_pub_key() ) {
    return set_err_msg( "missing mapping key pairfile ["
        + cptr->get_mapping_key_pair_file() + "]" );
  }
  return cptr->has_status( PC_PYTH_RPC_CONNECTED |
                           PC_PYTH_HAS_BLOCK_HASH |
                           PC_PYTH_HAS_MAPPING );
}

void upd_product::submit()
{
  manager *cptr = get_manager();
  key_pair *pkey = cptr->get_publish_key_pair();
  if ( !pkey ) {
    on_error_sub( "missing or invalid publish key [" +
        cptr->get_publish_key_pair_file() + "]", this );
    return;
  }
  pub_key *gpub = cptr->get_program_pub_key();
  if ( !gpub ) {
    on_error_sub( "missing or invalid program public key [" +
        cptr->get_program_pub_key_file() + "]", this );
    return;
  }
  pub_key *apub = prod_->get_account();
  if ( !cptr->get_account_key_pair( *apub, skey_ ) ) {
    std::string knm;
    apub->enc_base58( knm );
    on_error_sub( "missing key pair for product acct [" + knm + "]", this);
    return;
  }
  sreq_->set_program( gpub );
  sreq_->set_publish( pkey );
  sreq_->set_account( &skey_ );
  sreq_->set_sub( this );
  sig_->set_sub( this );

  // get recent block hash and submit request
  st_ = e_sent;
  sreq_->set_block_hash( get_manager()->get_recent_block_hash() );
  get_rpc_client()->send( sreq_ );
}

void upd_product::on_response( rpc::upd_product *res )
{
  if ( res->get_is_err() ) {
    on_error_sub( res->get_err_msg(), this );
    st_ = e_error;
  } else if ( st_ == e_sent ) {
    // subscribe to signature completion
    st_ = e_sig;
    sig_->set_commitment( cmt_ );
    sig_->set_signature( res->get_signature() );
    get_rpc_client()->send( sig_ );
  }
}

void upd_product::on_response( rpc::signature_subscribe *res )
{
  if ( res->get_is_err() ) {
    on_error_sub( res->get_err_msg(), this );
    st_ = e_error;
  } else if ( st_ == e_sig ) {
    st_ = e_done;
    on_response_sub( this );
  }
}

bool upd_product::get_is_done() const
{
  return st_ == e_done;
}

///////////////////////////////////////////////////////////////////////////
// add_price

add_price::add_price()
: st_( e_create_sent ),
  cmt_( commitment::e_confirmed )
{
}

void add_price::set_exponent( int32_t expo )
{
  sreq_->set_exponent( expo );
}

void add_price::set_price_type( price_type ptype )
{
  sreq_->set_price_type( ptype );
}

void add_price::set_product( product *prod )
{
  prod_ = prod;
}

void add_price::set_lamports( uint64_t funds )
{
  areq_->set_lamports( funds );
}

void add_price::set_commitment( commitment cmt )
{
  cmt_ = cmt;
}

key_pair *add_price::get_account()
{
  return &akey_;
}

bool add_price::get_is_ready()
{
  manager *cptr = get_manager();
  if ( !cptr->get_mapping_pub_key() ) {
    return set_err_msg( "missing mapping key pairfile ["
        + cptr->get_mapping_key_pair_file() + "]" );
  }
  return cptr->has_status( PC_PYTH_RPC_CONNECTED |
                           PC_PYTH_HAS_BLOCK_HASH |
                           PC_PYTH_HAS_MAPPING );
}

void add_price::submit()
{
  manager *cptr = get_manager();
  key_pair *pkey = cptr->get_publish_key_pair();
  if ( !pkey ) {
    on_error_sub( "missing or invalid publish key [" +
        cptr->get_publish_key_pair_file() + "]", this );
    return;
  }
  pub_key *gpub = cptr->get_program_pub_key();
  if ( !gpub ) {
    on_error_sub( "missing or invalid program public key [" +
        cptr->get_program_pub_key_file() + "]", this );
    return;
  }
  if ( prod_->get_price( sreq_->get_price_type() ) ) {
    on_error_sub( "price_type already exists for this product", this  );
    return;
  }
  pub_key *apub = prod_->get_account();
  if ( !cptr->get_account_key_pair( *apub, mkey_ ) ) {
    std::string knm;
    apub->enc_base58( knm );
    on_error_sub( "missing key pair for product acct [" + knm + "]", this);
    return;
  }
  if ( !cptr->create_account_key_pair( akey_ ) ) {
    on_error_sub( "failed to create new symbol key_pair", this );
    return;
  }
  areq_->set_sender( pkey );
  areq_->set_account( &akey_ );
  areq_->set_owner( gpub );
  areq_->set_space( sizeof( pc_price_t ) );
  sreq_->set_program( gpub );
  sreq_->set_publish( pkey );
  sreq_->set_product( &mkey_ );
  sreq_->set_account( &akey_ );
  areq_->set_sub( this );
  sreq_->set_sub( this );
  sig_->set_sub( this );

  // get recent block hash and submit request
  st_ = e_create_sent;
  areq_->set_block_hash( get_manager()->get_recent_block_hash() );
  get_rpc_client()->send( areq_ );
}

void add_price::on_response( rpc::create_account *res )
{
  if ( res->get_is_err() ) {
    on_error_sub( res->get_err_msg(), this );
    st_ = e_error;
  } else if ( st_ == e_create_sent ) {
    // subscribe to signature completion
    st_ = e_create_sig;
    sig_->set_commitment( commitment::e_finalized );
    sig_->set_signature( res->get_signature() );
    get_rpc_client()->send( sig_ );
  }
}

void add_price::on_response( rpc::signature_subscribe *res )
{
  if ( res->get_is_err() ) {
    on_error_sub( res->get_err_msg(), this );
    st_ = e_error;
  } else if ( st_ == e_create_sig ) {
    st_ = e_add_sent;
    sreq_->set_block_hash( get_manager()->get_recent_block_hash() );
    get_rpc_client()->send( sreq_ );
  } else if ( st_ == e_add_sig ) {
    st_ = e_done;
    on_response_sub( this );
  }
}

void add_price::on_response( rpc::add_price *res )
{
  if ( res->get_is_err() ) {
    on_error_sub( res->get_err_msg(), this );
    st_ = e_error;
  } else if ( st_ == e_add_sent ) {
    st_ = e_add_sig;
    sig_->set_commitment( cmt_ );
    sig_->set_signature( res->get_signature() );
    get_rpc_client()->send( sig_ );
  }
}

bool add_price::get_is_done() const
{
  return st_ == e_done;
}

///////////////////////////////////////////////////////////////////////////
// init_price

init_price::init_price()
: st_( e_init_sent ),
  cmt_( commitment::e_confirmed )
{
}

void init_price::set_exponent( int32_t expo )
{
  req_->set_exponent( expo );
}

void init_price::set_price( price *px )
{
  price_ = px;
}

void init_price::set_commitment( commitment cmt )
{
  cmt_ = cmt;
}

bool init_price::get_is_ready()
{
  manager *cptr = get_manager();
  if ( !cptr->get_mapping_pub_key() ) {
    return set_err_msg( "missing mapping key pairfile ["
        + cptr->get_mapping_key_pair_file() + "]" );
  }
  return cptr->has_status( PC_PYTH_RPC_CONNECTED |
                           PC_PYTH_HAS_BLOCK_HASH |
                           PC_PYTH_HAS_MAPPING );
}

void init_price::submit()
{
  manager *cptr = get_manager();
  key_pair *pkey = cptr->get_publish_key_pair();
  if ( !pkey ) {
    on_error_sub( "missing or invalid publish key [" +
        cptr->get_publish_key_pair_file() + "]", this );
    return;
  }
  pub_key *gpub = cptr->get_program_pub_key();
  if ( !gpub ) {
    on_error_sub( "missing or invalid program public key [" +
        cptr->get_program_pub_key_file() + "]", this );
    return;
  }
  pub_key *pub = price_->get_account();
  if ( !cptr->get_account_key_pair( *pub, key_ ) ) {
    std::string knm;
    pub->enc_base58( knm );
    on_error_sub( "missing key pair for price acct [" + knm + "]", this);
    return;
  }
  req_->set_program( gpub );
  req_->set_publish( pkey );
  req_->set_account( &key_ );
  req_->set_price_type( price_->get_price_type() );
  req_->set_sub( this );
  sig_->set_sub( this );

  // get recent block hash and submit request
  st_ = e_init_sent;
  req_->set_block_hash( get_manager()->get_recent_block_hash() );
  get_rpc_client()->send( req_ );
}

void init_price::on_response( rpc::signature_subscribe *res )
{
  if ( res->get_is_err() ) {
    on_error_sub( res->get_err_msg(), this );
    st_ = e_error;
  } else if ( st_ == e_init_sig ) {
    st_ = e_done;
    on_response_sub( this );
  }
}

void init_price::on_response( rpc::init_price *res )
{
  if ( res->get_is_err() ) {
    on_error_sub( res->get_err_msg(), this );
    st_ = e_error;
  } else if ( st_ == e_init_sent ) {
    st_ = e_init_sig;
    sig_->set_commitment( cmt_ );
    sig_->set_signature( res->get_signature() );
    get_rpc_client()->send( sig_ );
  }
}

bool init_price::get_is_done() const
{
  return st_ == e_done;
}

///////////////////////////////////////////////////////////////////////////
// add_publisher

add_publisher::add_publisher()
: st_( e_add_sent ),
  cmt_( commitment::e_confirmed ),
  px_( nullptr )
{
}

void add_publisher::set_price( price *px )
{
  px_ = px;
}

void add_publisher::set_publisher( const pub_key& pub )
{
  req_->set_publisher( pub );
}

void add_publisher::set_commitment( commitment cmt )
{
  cmt_ = cmt;
}

bool add_publisher::get_is_ready()
{
  manager *cptr = get_manager();
  if ( !cptr->get_mapping_pub_key() ) {
    return set_err_msg( "missing mapping key file ["
        + cptr->get_mapping_pub_key_file() + "]" );
  }
  return cptr->has_status( PC_PYTH_RPC_CONNECTED |
                           PC_PYTH_HAS_BLOCK_HASH |
                           PC_PYTH_HAS_MAPPING );
}

void add_publisher::submit()
{
  manager *cptr = get_manager();

  // check if we already have this symbol/publisher combination
  if ( px_->has_publisher( *req_->get_publisher() ) ) {
    on_error_sub( "publisher already exists for this symbol", this );
    return;
  }
  key_pair *pkey = cptr->get_publish_key_pair();
  if ( !pkey ) {
    on_error_sub( "missing or invalid publish key [" +
        cptr->get_publish_key_pair_file() + "]", this );
    return;
  }
  pub_key *gpub = cptr->get_program_pub_key();
  if ( !gpub ) {
    on_error_sub( "missing or invalid program public key [" +
        cptr->get_program_pub_key_file() + "]", this );
    return;
  }
  pub_key *apub = px_->get_account();
  if ( !cptr->get_account_key_pair( *apub, akey_ ) ) {
    std::string knm;
    apub->enc_base58( knm );
    on_error_sub( "missing key pair for price acct [" + knm + "]", this);
    return;
  }
  req_->set_program( gpub );
  req_->set_publish( pkey );
  req_->set_account( &akey_ );
  req_->set_sub( this );
  sig_->set_sub( this );

  // get recent block hash and submit request
  st_ = e_add_sent;
  req_->set_block_hash( get_manager()->get_recent_block_hash() );
  get_rpc_client()->send( req_ );
}

void add_publisher::on_response( rpc::add_publisher *res )
{
  if ( res->get_is_err() ) {
    on_error_sub( res->get_err_msg(), this );
    st_ = e_error;
  } else if ( st_ == e_add_sent ) {
    st_ = e_add_sig;
    sig_->set_commitment( cmt_ );
    sig_->set_signature( res->get_signature() );
    get_rpc_client()->send( sig_ );
  }
}

void add_publisher::on_response( rpc::signature_subscribe *res )
{
  if ( res->get_is_err() ) {
    on_error_sub( res->get_err_msg(), this );
    st_ = e_error;
  } else if ( st_ == e_add_sig ) {
    st_ = e_done;
    on_response_sub( this );
  }
}

bool add_publisher::get_is_done() const
{
  return st_ == e_done;
}

///////////////////////////////////////////////////////////////////////////
// del_publisher

del_publisher::del_publisher()
: st_( e_add_sent ),
  cmt_( commitment::e_confirmed ),
  px_( nullptr )
{
}

void del_publisher::set_price( price *px )
{
  px_ = px;
}

void del_publisher::set_publisher( const pub_key& pub )
{
  req_->set_publisher( pub );
}

void del_publisher::set_commitment( commitment cmt )
{
  cmt_ = cmt;
}

bool del_publisher::get_is_ready()
{
  manager *cptr = get_manager();
  if ( !cptr->get_mapping_pub_key() ) {
    return set_err_msg( "missing mapping key file ["
        + cptr->get_mapping_pub_key_file() + "]" );
  }
  return cptr->has_status( PC_PYTH_RPC_CONNECTED |
                           PC_PYTH_HAS_BLOCK_HASH |
                           PC_PYTH_HAS_MAPPING );
}

void del_publisher::submit()
{
  manager *cptr = get_manager();

  // check if we already have this symbol/publisher combination
  if ( !px_->has_publisher( *req_->get_publisher() ) ) {
    on_error_sub( "publisher not defined for this symbol", this );
    return;
  }
  key_pair *pkey = cptr->get_publish_key_pair();
  if ( !pkey ) {
    on_error_sub( "missing or invalid publish key [" +
        cptr->get_publish_key_pair_file() + "]", this );
    return;
  }
  pub_key *gpub = cptr->get_program_pub_key();
  if ( !gpub ) {
    on_error_sub( "missing or invalid program public key [" +
        cptr->get_program_pub_key_file() + "]", this );
    return;
  }
  pub_key *apub = px_->get_account();
  if ( !cptr->get_account_key_pair( *apub, akey_ ) ) {
    std::string knm;
    apub->enc_base58( knm );
    on_error_sub( "missing key pair for price acct [" + knm + "]", this);
    return;
  }
  req_->set_program( gpub );
  req_->set_publish( pkey );
  req_->set_account( &akey_ );
  req_->set_sub( this );
  sig_->set_sub( this );

  // get recent block hash and submit request
  st_ = e_add_sent;
  req_->set_block_hash( get_manager()->get_recent_block_hash() );
  get_rpc_client()->send( req_ );
}

void del_publisher::on_response( rpc::del_publisher *res )
{
  if ( res->get_is_err() ) {
    on_error_sub( res->get_err_msg(), this );
    st_ = e_error;
  } else if ( st_ == e_add_sent ) {
    st_ = e_add_sig;
    sig_->set_commitment( cmt_ );
    sig_->set_signature( res->get_signature() );
    get_rpc_client()->send( sig_ );
  }
}

void del_publisher::on_response( rpc::signature_subscribe *res )
{
  if ( res->get_is_err() ) {
    on_error_sub( res->get_err_msg(), this );
    st_ = e_error;
  } else if ( st_ == e_add_sig ) {
    st_ = e_done;
    on_response_sub( this );
  }
}

bool del_publisher::get_is_done() const
{
  return st_ == e_done;
}

///////////////////////////////////////////////////////////////////////////
// init_prm

init_prm::init_prm()
: st_( e_create_sent )
{
}

void init_prm::set_lamports( uint64_t lamports )
{
  creq_->set_lamports( lamports );
}

void init_prm::submit()
{
  // get keys
  manager *sptr = get_manager();
  key_pair *pkey = sptr->get_publish_key_pair();
  if ( !pkey ) {
    on_error_sub( "missing or invalid publish key [" +
        sptr->get_publish_key_pair_file(), this );
    return;
  }
  key_pair *akey = sptr->get_param_key_pair();
  if ( akey ) {
    on_error_sub( "param key pair already exists [" +
        sptr->get_param_key_pair_file() + "]", this );
    return;
  }
  akey = sptr->create_param_key_pair();
  if ( !akey ) {
    on_error_sub( "failed to create param key pair [" +
        sptr->get_param_key_pair_file() + "]", this );
    return;
  }
  pub_key *gpub = sptr->get_program_pub_key();
  if ( !gpub ) {
    on_error_sub( "missing or invalid program public key [" +
        sptr->get_program_pub_key_file() + "]", this );
    return;
  }
  creq_->set_sub( this );
  creq_->set_sender( pkey );
  creq_->set_account( akey );
  creq_->set_owner( gpub );
  creq_->set_space( sizeof( pc_prm_t ) );

  ireq_->set_sub( this );
  ireq_->set_publish( pkey );
  ireq_->set_param( akey );
  ireq_->set_program( gpub );

  ureq_->set_sub( this );
  ureq_->set_publish( pkey );
  ureq_->set_param( akey );
  ureq_->set_program( gpub );

  areq_->set_sub( this );
  areq_->set_account( sptr->get_param_pub_key() );

  sig_->set_sub( this );
  sig_->set_commitment( commitment::e_finalized );

  // get recent block hash and submit request
  st_ = e_create_sent;
  slot_ = 0UL;
  creq_->set_block_hash( get_manager()->get_recent_block_hash() );
  get_rpc_client()->send( creq_ );
}

void init_prm::on_response( rpc::create_account *res )
{
  if ( res->get_is_err() ) {
    on_error_sub( res->get_err_msg(), this );
    st_ = e_error;
  } else if ( st_ == e_create_sent ) {
    PC_LOG_DBG( "successful param account creation" ).end();
    // subscribe to signature completion
    st_ = e_create_sig;
    sig_->set_commitment( commitment::e_finalized );
    sig_->set_signature( res->get_signature() );
    get_rpc_client()->send( sig_ );
  }
}

void init_prm::on_response( rpc::init_prm *res )
{
  if ( res->get_is_err() ) {
    on_error_sub( res->get_err_msg(), this );
    st_ = e_error;
  } else if ( st_ == e_init_sent ) {
    // subscribe to signature completion
    PC_LOG_DBG( "successful param account initialize" ).end();
    st_ = e_init_sig;
    sig_->set_commitment( commitment::e_finalized );
    sig_->set_signature( res->get_signature() );
    get_rpc_client()->send( sig_ );
  }
}

void init_prm::on_response( rpc::upd_prm *res )
{
  if ( res->get_is_err() ) {
    on_error_sub( res->get_err_msg(), this );
    st_ = e_error;
  } else if ( st_ == e_upd_sent ) {
    st_ = e_pend_prm;
    slot_ = 1+get_manager()->get_slot();
  }
}

void init_prm::upd_norm(uint32_t idx )
{
  double denom = __builtin_sqrt( 2*M_PI);
  double x = idx * 0.001;
  ureq_->set_from( idx );
  for(unsigned i=0; i != PC_NORMAL_UPDATE; ++i ) {
    if ( idx < PC_NORMAL_SIZE ) {
      double inorm = __builtin_exp( -.5*x*x)/denom;
      ureq_->set_num( i+1 );
      ureq_->set_norm( i, inorm*1e9 );
      x += 0.001;
      ++idx;
    } else {
      ureq_->set_norm( i, 0.0 );
    }
  }
}

void init_prm::on_response( rpc::signature_subscribe *res )
{
  if ( res->get_is_err() ) {
    on_error_sub( res->get_err_msg(), this );
    st_ = e_error;
  } else if ( st_ == e_create_sig ) {
    PC_LOG_DBG( "completed account creation" ).end();
    st_ = e_init_sent;
    ireq_->set_block_hash( get_manager()->get_recent_block_hash() );
    get_rpc_client()->send( ireq_ );
  } else if ( st_ == e_init_sig ) {
    PC_LOG_DBG( "completed account initialize" ).end();
    st_ = e_get_prm;
    get_rpc_client()->send( areq_ );
  }
}

void init_prm::on_response( rpc::get_account_info *res )
{
  if ( res->get_is_err() ) {
    on_error_sub( res->get_err_msg(), this );
    st_ = e_error;
    return;
  }
  // decode and verify account values
  PC_LOG_DBG( "checking param account" ).end();
  manager *cptr = get_manager();
  pc_prm_t *prm;
  if ( sizeof( pc_prm_t ) > res->get_data( prm ) ||
       prm->magic_ != PC_MAGIC ||
       prm->type_ != PC_ACCTYPE_PARAMS ) {
    cptr->set_err_msg( "invalid or corrupt parameter account" );
    st_ = e_error;
    return;
  }
  if ( prm->ver_ != PC_VERSION ) {
    cptr->set_err_msg( "invalid parameter account version=" +
        std::to_string( prm->ver_ ) );
    st_ = e_error;
    return;
  }
  // verify account parameters
  uint64_t m = 1UL;
  for( uint32_t i=0;i != PC_FACTOR_SIZE; ++i ) {
    if ( prm->fact_[i] != m ) {
      cptr->set_err_msg( "invalid parameter account factor=" +
          std::to_string( prm->fact_[i] ) );
      st_ = e_error;
      return;
    }
    m *= 10L;
  }
  // check which values are missing
  for(unsigned i=0; i != PC_NORMAL_SIZE; ++i ) {
    if ( prm->norm_[i] == 0UL ) {
      // send norm values for this section
      uint64_t idx = i - (i % PC_NORMAL_UPDATE);
      st_ = e_upd_sent;
      PC_LOG_DBG( "updating param account" ).add( "idx", idx ).end();
      upd_norm( idx );
      ureq_->set_block_hash( get_manager()->get_recent_block_hash() );
      get_rpc_client()->send( ureq_ );
      return;
    }
  }
  st_ = e_done;
}

bool init_prm::get_is_done() const
{
  if ( st_ == e_done ) {
    return true;
  }
  if ( st_ == e_pend_prm ) {
    manager *cptr = get_manager();
    if ( slot_ < cptr->get_slot() ) {
      const_cast<state_t&>(st_) = e_get_prm;
      get_rpc_client()->send( const_cast<rpc::get_account_info*>(areq_) );
    }
  }
  return false;
}

///////////////////////////////////////////////////////////////////////////
// upd_prm

void upd_prm::submit()
{
  // get keys
  manager *sptr = get_manager();
  key_pair *pkey = sptr->get_publish_key_pair();
  if ( !pkey ) {
    on_error_sub( "missing or invalid publish key [" +
        sptr->get_publish_key_pair_file(), this );
    return;
  }
  key_pair *akey = sptr->get_param_key_pair();
  if ( !akey ) {
    on_error_sub( "missing param key pair [" +
        sptr->get_param_key_pair_file() + "]", this );
    return;
  }
  pub_key *gpub = sptr->get_program_pub_key();
  if ( !gpub ) {
    on_error_sub( "missing or invalid program public key [" +
        sptr->get_program_pub_key_file() + "]", this );
    return;
  }
  ureq_->set_sub( this );
  ureq_->set_publish( pkey );
  ureq_->set_param( akey );
  ureq_->set_program( gpub );

  areq_->set_sub( this );
  areq_->set_account( sptr->get_param_pub_key() );

  // get recent block hash and submit request
  st_ = e_get_prm;
  slot_ = 0UL;
  get_rpc_client()->send( areq_ );
}

///////////////////////////////////////////////////////////////////////////
// init_test

init_test::init_test()
: st_( e_create_sent )
{
}

void init_test::set_lamports( uint64_t lamports )
{
  creq_->set_lamports( lamports );
}

key_pair *init_test::get_account()
{
  return &tkey_;
}

void init_test::submit()
{
  // get keys
  manager *sptr = get_manager();
  key_pair *pkey = sptr->get_publish_key_pair();
  if ( !pkey ) {
    on_error_sub( "missing or invalid publish key [" +
        sptr->get_publish_key_pair_file(), this );
    return;
  }
  pub_key *gpub = sptr->get_program_pub_key();
  if ( !gpub ) {
    on_error_sub( "missing or invalid program public key [" +
        sptr->get_program_pub_key_file() + "]", this );
    return;
  }
  manager *cptr = get_manager();
  if ( !cptr->create_account_key_pair( tkey_ ) ) {
    on_error_sub( "failed to create new test key_pair", this );
    return;
  }

  creq_->set_sub( this );
  creq_->set_sender( pkey );
  creq_->set_account( &tkey_ );
  creq_->set_owner( gpub );
  creq_->set_space( sizeof( pc_test_t ) );

  ireq_->set_sub( this );
  ireq_->set_publish( pkey );
  ireq_->set_account( &tkey_ );
  ireq_->set_program( gpub );

  sig_->set_sub( this );
  sig_->set_commitment( commitment::e_finalized );

  // get recent block hash and submit request
  st_ = e_create_sent;
  creq_->set_block_hash( cptr->get_recent_block_hash() );
  get_rpc_client()->send( creq_ );
}

void init_test::on_response( rpc::create_account *res )
{
  if ( res->get_is_err() ) {
    on_error_sub( res->get_err_msg(), this );
    st_ = e_error;
  } else if ( st_ == e_create_sent ) {
    // subscribe to signature completion
    st_ = e_create_sig;
    sig_->set_commitment( commitment::e_finalized );
    sig_->set_signature( res->get_signature() );
    get_rpc_client()->send( sig_ );
  }
}

void init_test::on_response( rpc::signature_subscribe *res )
{
  if ( res->get_is_err() ) {
    on_error_sub( res->get_err_msg(), this );
    st_ = e_error;
  } else if ( st_ == e_create_sig ) {
    st_ = e_init_sent;
    ireq_->set_block_hash( get_manager()->get_recent_block_hash() );
    get_rpc_client()->send( ireq_ );
  } else if ( st_ == e_init_sig ) {
    st_ = e_done;
    on_response_sub( this );
  }
}

void init_test::on_response( rpc::init_test *res )
{
  if ( res->get_is_err() ) {
    on_error_sub( res->get_err_msg(), this );
    st_ = e_error;
  } else if ( st_ == e_init_sent ) {
    st_ = e_init_sig;
    sig_->set_signature( res->get_signature() );
    get_rpc_client()->send( sig_ );
  }
}

bool init_test::get_is_done() const
{
  return st_ == e_done;
}

///////////////////////////////////////////////////////////////////////////
// upd_test

void upd_test::set_test_key( const std::string& tkey )
{
  tpub_.init_from_text( tkey );
}

bool upd_test::init_from_file( const std::string& file )
{
  mem_map mf;
  mf.set_file( file );
  if ( !mf.init() ) {
    return set_err_msg( "failed to read file[" + file + "]" );
  }
  jtree pt;
  pt.parse( mf.data(), mf.size() );
  if ( !pt.is_valid() ) {
    return set_err_msg( "failed to parse file[" + file + "]" );
  }
  ureq_->set_expo( pt.get_int( pt.find_val( 1, "exponent" ) ) );
  ureq_->set_slot_diff( pt.get_int( pt.find_val( 1, "slot_diff" ) ) );
  unsigned i=0, qt =pt.find_val( 1, "quotes" );
  for( uint32_t it = pt.get_first( qt ); it; it = pt.get_next( it ) ) {
    int64_t px = pt.get_int( pt.find_val( it,  "price"  ) );
    int64_t conf = pt.get_uint( pt.find_val( it, "conf" ) );
    ureq_->set_price( i, px, conf );
    ++i;
  }
  return true;
}

bool upd_test::get_is_done() const
{
  return st_ == e_done;
}

void upd_test::submit()
{
  // get keys
  manager *sptr = get_manager();
  key_pair *pkey = sptr->get_publish_key_pair();
  if ( !pkey ) {
    on_error_sub( "missing or invalid publish key [" +
        sptr->get_publish_key_pair_file(), this );
    return;
  }
  pub_key *akey = sptr->get_param_pub_key();
  if ( !akey ) {
    on_error_sub( "missing param pulic bkey [" +
        sptr->get_param_key_pair_file() + "]", this );
    return;
  }
  pub_key *gpub = sptr->get_program_pub_key();
  if ( !gpub ) {
    on_error_sub( "missing or invalid program public key [" +
        sptr->get_program_pub_key_file() + "]", this );
    return;
  }
  manager *cptr = get_manager();
  if ( !cptr->get_account_key_pair( tpub_, tkey_ ) ) {
    on_error_sub( "failed to find test key_pair", this );
    return;
  }

  ureq_->set_sub( this );
  ureq_->set_publish( pkey );
  ureq_->set_param( akey );
  ureq_->set_account( &tkey_ );
  ureq_->set_program( gpub );

  areq_->set_sub( this );
  areq_->set_account( &tpub_ );
  areq_->set_commitment( commitment::e_confirmed );
  get_rpc_client()->send( areq_ );

  // get recent block hash and submit request
  st_ = e_upd_sent;
  ureq_->set_block_hash( cptr->get_recent_block_hash() );
  get_rpc_client()->send( ureq_ );
}

void upd_test::on_response( rpc::upd_test *res )
{
  if ( res->get_is_err() ) {
    on_error_sub( res->get_err_msg(), this );
    st_ = e_error;
  }
}

void upd_test::on_response( rpc::account_subscribe *res )
{
  if ( res->get_is_err() ) {
    on_error_sub( res->get_err_msg(), this );
    st_ = e_error;
    return;
  }
  manager *cptr = get_manager();
  pc_test_t *tptr;
  if ( sizeof( pc_test_t ) > res->get_data( tptr ) ||
       tptr->magic_ != PC_MAGIC ||
       tptr->type_ != PC_ACCTYPE_TEST ) {
    cptr->set_err_msg( "invalid or corrupt test account" );
    st_ = e_error;
    return;
  }
  if ( tptr->ver_ != PC_VERSION ) {
    cptr->set_err_msg( "invalid test account version=" +
        std::to_string( tptr->ver_ ) );
    st_ = e_error;
    return;
  }
  json_wtr jw;
  jw.add_val( json_wtr::e_obj );
  jw.add_key( "exponent", (int64_t)tptr->expo_ );
  jw.add_key( "price", tptr->price_ );
  jw.add_key( "conf", tptr->conf_ );
  jw.pop();
  net_buf *hd, *tl;
  jw.detach( hd, tl );
  while( hd ) {
    net_buf *nxt = hd->next_;
    std::cout.write( hd->buf_, hd->size_ );
    hd = nxt;
  }
  std::cout << std::endl;
  st_ = e_done;
}

///////////////////////////////////////////////////////////////////////////
// transfer

transfer::transfer()
: st_( e_sent ),
  cmt_( commitment::e_confirmed )
{
}

void transfer::set_receiver( pub_key *pkey )
{
  req_->set_receiver( pkey );
}

void transfer::set_lamports( uint64_t funds )
{
  req_->set_lamports( funds );
}

void transfer::set_commitment( commitment cmt )
{
  cmt_ = cmt;
}

void transfer::on_response( rpc::transfer *res )
{
  if ( res->get_is_err() ) {
    on_error_sub( res->get_err_msg(), this );
    st_ = e_error;
  } else if ( st_ == e_sent ) {
    st_ = e_sig;
    sig_->set_commitment( cmt_ );
    sig_->set_signature( res->get_signature() );
    get_rpc_client()->send( sig_ );
  }
}

void transfer::on_response( rpc::signature_subscribe *res )
{
  if ( res->get_is_err() ) {
    on_error_sub( res->get_err_msg(), this );
    st_ = e_error;
  } else if ( st_ == e_sig ) {
    st_ = e_done;
    on_response_sub( this );
  }
}

void transfer::submit()
{
  manager *cptr = get_manager();
  key_pair *pkey = cptr->get_publish_key_pair();
  if ( !pkey ) {
    on_error_sub( "missing or invalid publish key [" +
        cptr->get_publish_key_pair_file() + "]", this );
    return;
  }
  req_->set_sender( pkey );
  req_->set_sub( this );
  sig_->set_sub( this );

  // get recent block hash and submit request
  st_ = e_sent;
  req_->set_block_hash( cptr->get_recent_block_hash() );
  get_rpc_client()->send( req_ );
}

bool transfer::get_is_done() const
{
  return st_ == e_done;
}

///////////////////////////////////////////////////////////////////////////
// balance

balance::balance()
: st_( e_sent ),
  pub_( nullptr )
{
}

void balance::set_pub_key( pub_key *pkey )
{
  pub_ = pkey;
}

uint64_t balance::get_lamports() const
{
  return req_->get_lamports();
}

void balance::on_response( rpc::get_account_info *res )
{
  if ( res->get_is_err() ) {
    on_error_sub( res->get_err_msg(), this );
    st_ = e_error;
  } else if ( st_ == e_sent ) {
    st_ = e_done;
    on_response_sub( this );
  }
}

void balance::submit()
{
  if ( pub_ ) {
    req_->set_account( pub_ );
  } else {
    manager *cptr = get_manager();
    pub_key *pkey = cptr->get_publish_pub_key();
    if ( !pkey ) {
      on_error_sub( "missing or invalid publish key [" +
          cptr->get_publish_key_pair_file() + "]", this );
      return;
    }
    req_->set_account( pkey );
  }
  st_ = e_sent;
  req_->set_sub( this );
  get_rpc_client()->send( req_ );
}

bool balance::get_is_done() const
{
  return st_ == e_done;
}

///////////////////////////////////////////////////////////////////////////
// get_minimum_balance_rent_exemption

get_minimum_balance_rent_exemption::get_minimum_balance_rent_exemption()
: st_( e_sent )
{
}

void get_minimum_balance_rent_exemption::set_size( size_t sz )
{
  req_->set_size( sz );
}

uint64_t get_minimum_balance_rent_exemption::get_lamports() const
{
  return req_->get_lamports();
}

void get_minimum_balance_rent_exemption::on_response(
    rpc::get_minimum_balance_rent_exemption *res )
{
  if ( res->get_is_err() ) {
    on_error_sub( res->get_err_msg(), this );
    st_ = e_error;
  } else if ( st_ == e_sent ) {
    st_ = e_done;
    on_response_sub( this );
  }
}

void get_minimum_balance_rent_exemption::submit()
{
  st_ = e_sent;
  req_->set_sub( this );
  get_rpc_client()->send( req_ );
}

bool get_minimum_balance_rent_exemption::get_is_done() const
{
  return st_ == e_done;
}

///////////////////////////////////////////////////////////////////////////
// product

product::product( const pub_key& acc )
: acc_( acc ),
  st_( e_subscribe )
{
  areq_->set_account( &acc_ );
  sreq_->set_account( &acc_ );
  areq_->set_sub( this );
  sreq_->set_sub( this );
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
}

void product::add_price( price *px )
{
  pvec_.push_back( px );
}

void product::submit()
{
  rpc_client  *cptr = get_rpc_client();
  st_ = e_subscribe;
  cptr->send( sreq_ );
  cptr->send( areq_ );
}

void product::on_response( rpc::get_account_info *res )
{
  update( res );
}

void product::on_response( rpc::account_subscribe *res )
{
  update( res );
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
  if ( sizeof( pc_prod_t ) > res->get_data( prod ) ||
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

///////////////////////////////////////////////////////////////////////////
// price

price::price( const pub_key& acc, product *prod )
: init_( false ),
  isched_( false ),
  st_( e_subscribe ),
  apub_( acc ),
  ptype_( price_type::e_unknown ),
  sym_st_( symbol_status::e_unknown ),
  version_( 0 ),
  pub_idx_( (unsigned)-1 ),
  apx_( 0 ),
  aconf_( 0 ),
  twap_( 0 ),
  avol_( 0 ),
  valid_slot_( 0 ),
  pub_slot_( 0 ),
  lamports_( 0UL ),
  aexpo_( 0 ),
  cnum_( 0 ),
  prod_( prod ),
  sched_( this )
{
  __builtin_memset( &cpub_, 0, sizeof( cpub_ ) );
  areq_->set_account( &apub_ );
  sreq_->set_account( &apub_ );
  preq_->set_account( &apub_ );
  areq_->set_sub( this );
  sreq_->set_sub( this );
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
  pub_key *rkey = cptr->get_param_pub_key();
  if ( !rkey ) {
    on_error_sub( "missing or invalid param public key [" +
        cptr->get_param_pub_key_file() + "]", this );
    return false;
  }

  preq_->set_publish( pkey );
  preq_->set_pubcache( cptr->get_publish_key_cache() );
  preq_->set_program( gpub );
  preq_->set_params( rkey );
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

uint32_t price::get_version() const
{
  return version_;
}

int64_t  price::get_price() const
{
  return apx_;
}

int64_t price::get_twap() const
{
  return twap_;
}

uint64_t price::get_ann_volatility() const
{
  return avol_;
}

uint64_t price::get_conf() const
{
  return aconf_;
}

int64_t price::get_price_exponent() const
{
  return aexpo_;
}

price_type price::get_price_type() const
{
  return ptype_;
}

symbol_status price::get_status() const
{
  return sym_st_;
}

uint64_t price::get_lamports() const
{
  return lamports_;
}

uint64_t price::get_valid_slot() const
{
  return valid_slot_;
}

uint64_t price::get_pub_slot() const
{
  return pub_slot_;
}

bool price::get_is_ready_publish() const
{
  return st_ == e_publish && get_manager()->get_is_tx_connect();
}

void price::reset()
{
  st_ = e_subscribe;
  reset_err();
}

bool price::has_publisher()
{
  return pub_idx_ != (unsigned)-1;
}

bool price::has_publisher( const pub_key& key )
{
  pc_pub_key_t *pk = (pc_pub_key_t*)key.data();
  for(uint32_t i=0; i != cnum_; ++i ) {
    pc_pub_key_t *ikey = &cpub_[i];
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
  add_send( mgr->get_slot(), mgr->get_curr_time() );
  preq_->set_price( price, conf, st, mgr->get_slot(), is_agg );
  preq_->set_block_hash( mgr->get_recent_block_hash() );
  mgr->submit( preq_ );
  return true;
}

void price::submit()
{
  if ( st_ == e_subscribe ) {
    // subscribe first
    rpc_client  *cptr = get_rpc_client();
    cptr->send( sreq_ );
    cptr->send( areq_ );
    st_ = e_sent_subscribe;
  }
}

void price::on_response( rpc::get_account_info *res )
{
  update( res );
}

void price::on_response( rpc::account_subscribe *res )
{
  update( res );
}

void price::log_update( const char *title )
{
  PC_LOG_INF( title )
    .add( "account", *get_account() )
    .add( "product", *prod_->get_account() )
    .add( "symbol", get_symbol() )
    .add( "price_type", price_type_to_str( ptype_ ) )
    .add( "version", version_ )
    .add( "exponent", aexpo_ )
    .add( "num_publishers", cnum_ )
    .end();
}

void price::unsubscribe()
{
}

void price::init_price( pc_price_t *pupd )
{
  // (re) initialize all values
  aexpo_   = pupd->expo_;
  ptype_   = (price_type)pupd->ptype_;
  version_ = pupd->ver_;
  apx_     = pupd->agg_.price_;
  aconf_   = pupd->agg_.conf_;
  twap_    = pupd->twap_;
  avol_    = pupd->avol_;
  sym_st_  = (symbol_status)pupd->agg_.status_;
  pub_slot_   = pupd->agg_.pub_slot_;
  valid_slot_ = pupd->valid_slot_;
  cnum_ = pupd->num_;
  for( unsigned i=0; i != cnum_; ++i ) {
    if ( !pc_pub_key_equal( &cpub_[i], &pupd->comp_[i].pub_ ) ) {
      pc_pub_key_assign( &cpub_[i], &pupd->comp_[i].pub_ );
    }
    cprice_[i] = pupd->comp_[i].agg_;
  }
  // log new price object
  log_update( "init_price" );
}

void price::init_subscribe( pc_price_t *pupd )
{
  // update initial values
  aexpo_   = pupd->expo_;
  ptype_   = (price_type)pupd->ptype_;
  version_ = pupd->ver_;
  st_ = e_publish;

  // initial assignment
  cnum_ = pupd->num_;
  manager *cptr = get_manager();
  for( unsigned i=0; i != cnum_; ++i ) {
    pc_pub_key_assign( &cpub_[i], &pupd->comp_[i].pub_ );
  }
  update_pub();
  apx_  = pupd->agg_.price_;
  aconf_ = pupd->agg_.conf_;
  sym_st_ = (symbol_status)pupd->agg_.status_;
  pub_slot_ = pupd->agg_.pub_slot_;
  valid_slot_ = pupd->valid_slot_;

  // subscribe to next symbol in chain
  if ( !pc_pub_key_is_zero( &pupd->next_ ) ) {
    cptr->add_price( *(pub_key*)&pupd->next_, prod_ );
  }

  // log new price object
  log_update( "add_price" );

  // callback users on new symbol update
  manager_sub *sub = cptr->get_manager_sub();
  if ( sub ) {
    sub->on_add_symbol( cptr, this );
  }

  // reduce subscription count after we subscribe to next symbol in chain
  // do this last in case this triggers an init callback
  cptr->del_map_sub();
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
  pc_price_t *pupd;
  res->get_data( pupd );
  if ( PC_UNLIKELY( pupd->magic_ != PC_MAGIC ) ) {
    on_error_sub( "bad price account header", this );
    st_ = e_error;
    return;
  }

  // price account was (re) initialized
  if ( PC_UNLIKELY( pupd->valid_slot_ == 0L ) ) {
    init_price( pupd );
  }

  // update state and subscribe to next price account in the chain
  if ( PC_UNLIKELY( st_ == e_sent_subscribe ) ) {
    init_subscribe( pupd );
  }

  // update publishers and publisher prices
  lamports_ = res->get_lamports();
  if ( PC_UNLIKELY( cnum_ != pupd->num_ ) ) {
    cnum_ = pupd->num_;
    log_update( "modify_publisher" );
  }
  manager *mgr = get_manager();
  bool upd_pub = false;
  for( unsigned i=0; i != cnum_; ++i ) {
    cprice_[i] = pupd->comp_[i].agg_;
    if ( PC_UNLIKELY( !pc_pub_key_equal(
            &cpub_[i], &pupd->comp_[i].pub_ ) )  ){
      pc_pub_key_assign( &cpub_[i], &pupd->comp_[i].pub_ );
      upd_pub = true;
    }
  }
  if ( PC_UNLIKELY( upd_pub ) ) {
    update_pub();
  }

  // update aggregate price and status if changed
  if ( valid_slot_ != pupd->valid_slot_ || valid_slot_ == 0UL ) {
    apx_  = pupd->agg_.price_;
    aconf_ = pupd->agg_.conf_;
    twap_  = pupd->twap_;
    avol_   = pupd->avol_;
    sym_st_ = (symbol_status)pupd->agg_.status_;
    pub_slot_ = pupd->agg_.pub_slot_;
    valid_slot_ = pupd->valid_slot_;

    // capture aggregate price and components to disk
    mgr->write( (pc_pub_key_t*)apub_.data(), (pc_acc_t*)pupd );

    // add slot/time latency statistics
    if ( pub_idx_ != (unsigned)-1 ) {
      uint64_t pub_slot = cprice_[pub_idx_].pub_slot_;
      add_recv( mgr->get_slot(), pub_slot, res->get_recv_time() );
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
  for( unsigned i=0; i != cnum_; ++i ) {
    if ( pc_pub_key_equal( &cpub_[i], (pc_pub_key_t*)pkey ) ) {
      pub_idx_ = i;
    }
  }
}

bool price::get_is_done() const
{
  return st_ == e_publish;
}

unsigned price::get_num_publisher() const
{
  return cnum_;
}

const pub_key *price::get_publisher( unsigned i ) const
{
  return (const pub_key*)&cpub_[i];
}

int64_t price::get_publisher_price( unsigned i ) const
{
  return cprice_[i].price_;
}

uint64_t price::get_publisher_conf( unsigned i ) const
{
  return cprice_[i].conf_;
}

uint64_t price::get_publisher_slot( unsigned i ) const
{
  return cprice_[i].pub_slot_;
}

symbol_status price::get_publisher_status( unsigned i ) const
{
  return (symbol_status)cprice_[i].status_;
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

