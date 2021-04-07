#include "request.hpp"
#include "manager.hpp"
#include "log.hpp"
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
  return num_sym_ >= PC_MAP_NODE_SIZE;
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
  if ( !res->get_data( tab ) ) {
    cptr->set_err_msg( "invalid mapping account size" );
    return;
  }

  // check and subscribe to any new price accounts in mapping table
  num_sym_ = tab->num_;
  PC_LOG_INF( "add_mapping" )
    .add( "key_name", mkey_ )
    .add( "num_symbols", num_sym_ )
    .end();
  for( unsigned i=0; i != tab->num_; ++i ) {
    pc_map_node_t *nptr = &tab->nds_[i];
    pub_key *aptr = (pub_key*)&nptr->price_acc_;
    cptr->add_symbol( *aptr );
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
// add_symbol

add_symbol::add_symbol()
: st_( e_create_sent ),
  cmt_( commitment::e_confirmed )
{
}

void add_symbol::set_symbol( const symbol& sym )
{
  sreq_->set_symbol( sym );
}

void add_symbol::set_exponent( int32_t expo )
{
  sreq_->set_exponent( expo );
}

void add_symbol::set_price_type( price_type ptype )
{
  sreq_->set_price_type( ptype );
}

void add_symbol::set_lamports( uint64_t funds )
{
  areq_->set_lamports( funds );
}

void add_symbol::set_commitment( commitment cmt )
{
  cmt_ = cmt;
}

bool add_symbol::get_is_ready()
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

void add_symbol::submit()
{
  manager *cptr = get_manager();
  price *sptr = cptr->get_symbol(
      *sreq_->get_symbol(), sreq_->get_price_type() );
  if ( sptr && sptr->get_version() >= PC_VERSION ) {
    on_error_sub( "symbol/price type already exists with version="
        + std::to_string( sptr->get_version() ), this );
    return;
  }
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

void add_symbol::on_response( rpc::create_account *res )
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

void add_symbol::on_response( rpc::signature_subscribe *res )
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

void add_symbol::on_response( rpc::add_symbol *res )
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

bool add_symbol::get_is_done() const
{
  return st_ == e_done;
}

///////////////////////////////////////////////////////////////////////////
// add_publisher

add_publisher::add_publisher()
: st_( e_add_sent ),
  cmt_( commitment::e_confirmed )
{
}

void add_publisher::set_symbol( const symbol& sym )
{
  req_->set_symbol( sym );
}

void add_publisher::set_publisher( const pub_key& pub )
{
  req_->set_publisher( pub );
}

void add_publisher::set_price_type( price_type ptype )
{
  req_->set_price_type( ptype );
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

  // check if this symbol exists
  price *sptr = cptr->get_symbol(
      *req_->get_symbol(), req_->get_price_type() );
  if ( !sptr ) {
    on_error_sub( "symbol/price_type does not exist", this );
    return;
  }

  // check if we already have this symbol/publisher combination
  if ( sptr->has_publisher( *req_->get_publisher() ) ) {
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
  pub_key *apub = sptr->get_account();
  if ( !cptr->get_account_key_pair( *apub, akey_ ) ) {
    std::string knm;
    apub->enc_base58( knm );
    on_error_sub( "missing key pair for symbol [" + knm + "]", this);
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
  cmt_( commitment::e_confirmed )
{
}

void del_publisher::set_symbol( const symbol& sym )
{
  req_->set_symbol( sym );
}

void del_publisher::set_publisher( const pub_key& pub )
{
  req_->set_publisher( pub );
}

void del_publisher::set_price_type( price_type ptype )
{
  req_->set_price_type( ptype );
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

  // check if this symbol exists
  price *sptr = cptr->get_symbol(
      *req_->get_symbol(), req_->get_price_type() );
  if ( !sptr ) {
    on_error_sub( "symbol/price_type does not exist", this );
    return;
  }

  // check if we already have this symbol/publisher combination
  if ( !sptr->has_publisher( *req_->get_publisher() ) ) {
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
  pub_key *apub = sptr->get_account();
  if ( !cptr->get_account_key_pair( *apub, akey_ ) ) {
    std::string knm;
    apub->enc_base58( knm );
    on_error_sub( "missing key pair for symbol [" + knm + "]", this);
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
// price

price::price( const pub_key& acc )
: init_( false ),
  isched_( false ),
  st_( e_subscribe ),
  apub_( acc ),
  ptype_( price_type::e_unknown ),
  sym_st_( symbol_status::e_unknown ),
  version_( 0 ),
  apx_( 0 ),
  aconf_( 0 ),
  valid_slot_( 0 ),
  pub_slot_( 0 ),
  lamports_( 0UL ),
  aexpo_( 0 ),
  cnum_( 0 ),
  pkey_( nullptr ),
  sched_( this )
{
  __builtin_memset( &cpub_, 0, sizeof( cpub_ ) );
  areq_->set_account( &apub_ );
  sreq_->set_account( &apub_ );
  preq_->set_symbol( &sym_ );
  preq_->set_account( &apub_ );
  areq_->set_sub( this );
  sreq_->set_sub( this );
  preq_->set_sub( this );
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
  pkey_ = cptr->get_publish_pub_key();
  preq_->set_publish( pkey );
  preq_->set_program( gpub );
  init_ = true;
  return true;
}

symbol *price::get_symbol()
{
  return &sym_;
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
  return st_ == e_publish;
}

void price::reset()
{
  st_ = e_subscribe;
  reset_err();
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

bool price::update(
    int64_t price, uint64_t conf, symbol_status st )
{
  if ( PC_UNLIKELY( !init_ && !init_publish() ) ) {
    return false;
  }
  if ( PC_UNLIKELY( !has_publisher( *pkey_ ) ) ) {
    return set_err_msg( "not permissioned to update price" );
  }
  preq_->set_price( price, conf, st );
  manager *cptr = get_manager();
  if ( cptr && st_ == e_publish ) {
    st_ = e_pend_publish;
    cptr->submit( this );
    return true;
  } else {
    return false;
  }
}

void price::submit()
{
  manager *pptr = get_manager();
  rpc_client  *cptr = get_rpc_client();
  if ( st_ == e_pend_publish ) {
    if ( PC_UNLIKELY( get_is_err() ) ) {
      return;
    }
    st_ = e_sent_publish;
    preq_->set_block_hash( pptr->get_recent_block_hash() );
    cptr->send( preq_ );
  } else if ( st_ == e_subscribe ) {
    // subscribe first
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

void price::on_response( rpc::upd_price *res )
{
  if ( res->get_is_err() ) {
    on_error_sub( res->get_err_msg(), this );
    st_ = e_error;
    return;
  } else if ( st_ == e_sent_publish ) {
    st_ = e_publish;
  }
}

void price::init_subscribe( pc_price_t *pupd )
{
  // update initial values
  aexpo_   = pupd->expo_;
  ptype_   = (price_type)pupd->ptype_;
  version_ = pupd->ver_;
  sym_     = *(symbol*)&pupd->sym_;
  preq_->set_price_type( ptype_ );
  st_ = e_publish;
  PC_LOG_INF( "add_symbol" )
    .add( "symbol", sym_ )
    .add( "price_type", price_type_to_str( ptype_ ) )
    .add( "version", version_ )
    .add( "exponent", aexpo_ )
    .add( "num_publishers", pupd->num_ )
    .end();

  // initial assignment
  cnum_ = pupd->num_;
  for( unsigned i=0; i != cnum_; ++i ) {
    pc_pub_key_assign( &cpub_[i], &pupd->comp_[i].pub_ );
  }
  apx_  = pupd->agg_.price_;
  aconf_ = pupd->agg_.conf_;
  sym_st_ = (symbol_status)pupd->agg_.status_;
  pub_slot_ = pupd->agg_.pub_slot_;
  valid_slot_ = pupd->valid_slot_;

  // replace symbol/price_type with later version
  manager *cptr = get_manager();
  cptr->add_symbol( sym_, ptype_, this );

  // subscribe to next symbol in chain
  if ( !pc_pub_key_is_zero( &pupd->next_ ) ) {
    cptr->add_symbol( *(pub_key*)&pupd->next_ );
  }

  // callback users on new symbol update
  manager_sub *sub = cptr->get_manager_sub();
  if ( sub ) {
    sub->on_add_symbol( cptr, this );
  }

  // reduce subscription count after we subscribe to next symbol in chain
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
  if ( !res->get_data( pupd ) ) {
    on_error_sub( "invalid price account size", this );
    st_ = e_error;
    return;
  }

  // update state and subscribe to next price account in the chain
  if ( PC_UNLIKELY( st_ == e_sent_subscribe ) ) {
    init_subscribe( pupd );
  }

  // update publishers
  cnum_ = pupd->num_;
  lamports_ = res->get_lamports();
  for( unsigned i=0; i != cnum_; ++i ) {
    if ( !pc_pub_key_equal( &cpub_[i], &pupd->comp_[i].pub_ ) ) {
      pc_pub_key_assign( &cpub_[i], &pupd->comp_[i].pub_ );
    }
    cprice_[i] = pupd->comp_[i].agg_.price_;
    cconf_[i]  = pupd->comp_[i].agg_.conf_;
  }

  // update aggregate price and status if changed
  if ( valid_slot_ != pupd->valid_slot_ || valid_slot_ == 0UL ) {
    apx_  = pupd->agg_.price_;
    aconf_ = pupd->agg_.conf_;
    sym_st_ = (symbol_status)pupd->agg_.status_;
    pub_slot_ = pupd->agg_.pub_slot_;
    valid_slot_ = pupd->valid_slot_;

    // capture aggregate price and components to disk
    get_manager()->write( pupd );

    // ping subscribers with new aggregate price
    on_response_sub( this );
  }
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
  return cprice_[i];
}

uint64_t price::get_publisher_conf( unsigned i ) const
{
  return cconf_[i];
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
  uint64_t *iptr = (uint64_t*)ptr_->get_symbol()->data();
  shash_ = ((iptr[0]^iptr[1])<<3) |(uint64_t)ptr_->get_price_type();
  shash_ = shash_ % fraction;
  get_manager()->schedule( this );
}

void price_sched::schedule()
{
  if ( ptr_->get_is_ready_publish() ) {
    on_response_sub( this );
  }
}
