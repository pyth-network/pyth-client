#include "admin_request.hpp"
#include <pc/manager.hpp>
#include "pc/mem_map.hpp"
#include <iostream>

using namespace pc;

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
// set_min_pub_req

set_min_pub_req::set_min_pub_req()
: st_( e_init_sent ),
  cmt_( commitment::e_confirmed )
{
}

void set_min_pub_req::set_min_pub( uint8_t min_pub )
{
  req_->set_min_pub( min_pub );
}

void set_min_pub_req::set_price( price *px )
{
  price_ = px;
}

void set_min_pub_req::set_commitment( commitment cmt )
{
  cmt_ = cmt;
}

bool set_min_pub_req::get_is_ready()
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

void set_min_pub_req::submit()
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
  req_->set_sub( this );
  sig_->set_sub( this );

  // get recent block hash and submit request
  st_ = e_init_sent;
  req_->set_block_hash( get_manager()->get_recent_block_hash() );
  get_rpc_client()->send( req_ );
}

void set_min_pub_req::on_response( rpc::signature_subscribe *res )
{
  if ( res->get_is_err() ) {
    on_error_sub( res->get_err_msg(), this );
    st_ = e_error;
  } else if ( st_ == e_init_sig ) {
    st_ = e_done;
    on_response_sub( this );
  }
}

void set_min_pub_req::on_response( rpc::set_min_pub_rpc *res )
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

bool set_min_pub_req::get_is_done() const
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
    uint64_t conf = pt.get_uint( pt.find_val( it, "conf" ) );
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

