#include "request.hpp"
#include "manager.hpp"
#include "mem_map.hpp"
#include "log.hpp"
#include <algorithm>
#include <math.h>
#include <iostream>
#include <zstd.h>

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
  cb_( nullptr ),
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

void request::on_response( rpc::program_subscribe * )
{
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

void get_mapping::on_response( rpc::program_subscribe *res )
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
  areq_->set_space( PC_PROD_ACC_SIZE );
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
  creq_->set_space( sizeof( pc_price_t ) );

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
  unsigned i=0, qt =pt.find_val( 1, "quotes" );
  for( uint32_t it = pt.get_first( qt ); it; it = pt.get_next( it ) ) {
    int64_t px = pt.get_int( pt.find_val( it,  "price"  ) );
    int64_t conf = pt.get_uint( pt.find_val( it, "conf" ) );
    int64_t sdiff = pt.get_int( pt.find_val( it, "slot_diff" ) );
    ureq_->set_price( i++, px, conf, sdiff );
  }
  ureq_->set_num( i );
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
  pc_price_t *tptr;
  if ( sizeof( pc_price_t ) != res->get_data_ref( tptr ) ||
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
  jw.add_key( "price", tptr->agg_.price_ );
  jw.add_key( "conf", tptr->agg_.conf_ );
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
  req_->set_commitment( get_manager()->get_commitment() );
  get_rpc_client()->send( req_ );
}

bool balance::get_is_done() const
{
  return st_ == e_done;
}

///////////////////////////////////////////////////////////////////////////
// get_block

get_block::get_block()
: st_( e_sent )
{
}

void get_block::set_slot( uint64_t slot )
{
  req_->set_slot( slot );
}

void get_block::set_commitment( commitment cmt )
{
  req_->set_commitment( cmt );
}

void get_block::on_response( rpc::get_block *res )
{
  if ( res->get_is_err() ) {
    on_error_sub( res->get_err_msg(), this );
    st_ = e_error;
  } else if ( st_ == e_sent && res->get_is_end() ) {
    st_ = e_done;
  }
}

bool get_block::get_is_done() const
{
  return st_ == e_done;
}

void get_block::submit()
{
  st_ = e_sent;
  req_->set_sub( this );
  req_->set_program( get_manager()->get_program_pub_key() );
  get_rpc_client()->send( req_ );
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

void product::on_response( rpc::program_subscribe *res )
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
  return st_ == e_publish && get_manager()->get_is_tx_connect();
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
  preq_->set_price( price, conf, st, mgr->get_slot(), is_agg );
  preq_->set_block_hash( mgr->get_recent_block_hash() );
  mgr->submit( preq_ );
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

void price::on_response( rpc::get_account_info *res )
{
  set_is_recv( true );
  update( res );
}

void price::on_response( rpc::program_subscribe *res )
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
  for( unsigned i=0; i != pptr_->num_; ++i ) {
    if ( pc_pub_key_equal( &pptr_->comp_[i].pub_, (pc_pub_key_t*)pkey ) ) {
      pub_idx_ = i;
      break;
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
