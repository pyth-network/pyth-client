#include "pyth_request.hpp"
#include "pyth_client.hpp"
#include "log.hpp"

using namespace pc;

///////////////////////////////////////////////////////////////////////////
// pyth_request

pyth_request::pyth_request()
: svr_( nullptr ),
  clnt_( nullptr ),
  cb_( nullptr )
{
}

void pyth_request::set_pyth_client( pyth_client *svr )
{
  svr_ = svr;
}

pyth_client *pyth_request::get_pyth_client() const
{
  return svr_;
}

void pyth_request::set_rpc_client( rpc_client *clnt )
{
  clnt_ = clnt;
}

rpc_client *pyth_request::get_rpc_client() const
{
  return clnt_;
}

void pyth_request::set_sub( rpc_sub *cb )
{
  cb_ = cb;
}

rpc_sub *pyth_request::get_sub() const
{
  return cb_;
}

bool pyth_request::has_status() const
{
   return svr_->has_status( PC_PYTH_RPC_CONNECTED |
                            PC_PYTH_HAS_BLOCK_HASH );
}

bool pyth_request::get_is_done() const
{
  return true;
}

template<class T>
void pyth_request::on_response_sub( T *req )
{
  rpc_sub_i<T> *iptr = dynamic_cast<rpc_sub_i<T>*>( req->get_sub() );
  if ( iptr ) {
    iptr->on_response( req );
  }
}

template<class T>
void pyth_request::on_error_sub( const std::string& emsg, T *req )
{
  set_err_msg( emsg );
  on_response_sub( req );
}

///////////////////////////////////////////////////////////////////////////
// pyth_init_mapping

void pyth_init_mapping::set_lamports( uint64_t lamports )
{
  creq_->set_lamports( lamports );
}

void pyth_init_mapping::submit()
{
  // get keys
  pyth_client *sptr = get_pyth_client();
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
  creq_->set_block_hash( get_pyth_client()->get_recent_block_hash() );
  get_rpc_client()->send( creq_ );
}

void pyth_init_mapping::on_response( rpc::create_account *res )
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

void pyth_init_mapping::on_response( rpc::init_mapping *res )
{
  if ( res->get_is_err() ) {
    on_error_sub( res->get_err_msg(), this );
    st_ = e_error;
  } else if ( st_ == e_init_sent ) {
    // subscribe to signature completion
    st_ = e_init_sig;
    sig_->set_commitment( commitment::e_finalized );
    sig_->set_signature( res->get_signature() );
    get_rpc_client()->send( sig_ );
  }
}

void pyth_init_mapping::on_response( rpc::signature_subscribe *res )
{
  if ( res->get_is_err() ) {
    on_error_sub( res->get_err_msg(), this );
    st_ = e_error;
  } else if ( st_ == e_create_sig ) {
    st_ = e_init_sent;
    ireq_->set_block_hash( get_pyth_client()->get_recent_block_hash() );
    get_rpc_client()->send( ireq_ );
  } else if ( st_ == e_init_sig ) {
    st_ = e_done;
    on_response_sub( this );
  }
}

bool pyth_init_mapping::get_is_done() const
{
  return st_ == e_done;
}

///////////////////////////////////////////////////////////////////////////
// pyth_get_mapping

void pyth_get_mapping::set_mapping_key( pub_key *mkey )
{
  mkey_ = mkey;
}

void pyth_get_mapping::submit()
{
  st_ = e_new;
  num_ = 0;
  areq_->set_account( mkey_ );
  sreq_->set_account( mkey_ );
  areq_->set_sub( this );
  sreq_->set_sub( this );
  get_rpc_client()->send( areq_ );
  get_rpc_client()->send( sreq_ );
}

void pyth_get_mapping::on_response( rpc::get_account_info *res )
{
  update( res );
}

void pyth_get_mapping::on_response( rpc::account_subscribe *res )
{
  update( res );
}

template<class T>
void pyth_get_mapping::update( T *res )
{
  pyth_client *cptr = get_pyth_client();
  if ( res->get_is_err() ) {
    cptr->set_err_msg( res->get_err_msg() );
    return;
  }
  pc_map_table_t tab[1];
  if ( !res->get_data( tab ) ) {
    cptr->set_err_msg( "invalid mapping account size" );
    return;
  }
  for( const pc_map_node_t *nptr = &tab->nds_[num_];
       num_ < tab->num_; ++num_, ++nptr ) {
    const symbol *sptr  = (const symbol*)&nptr->sym_;
    const pub_key *aptr = (const pub_key*)&nptr->acc_;
    cptr->add_symbol(*sptr, *aptr );
  }
  // set mapping cache flag
  if ( st_ == e_new ) {
    st_ = e_init;
    if ( pc_pub_key_is_zero( (pc_pub_key_t*)&tab->next_ ) ) {
      cptr->set_status( PC_PYTH_HAS_MAPPING );
    } else {
      // TODO: subscribe to next account in list
      // but not on reconnect
    }
  }
}

///////////////////////////////////////////////////////////////////////////
// pyth_add_symbol

void pyth_add_symbol::set_symbol( const symbol& sym )
{
  sreq_->set_symbol( sym );
}

void pyth_add_symbol::set_exponent( int32_t expo )
{
  sreq_->set_exponent( expo );
}

void pyth_add_symbol::set_lamports( uint64_t funds )
{
  areq_->set_lamports( funds );
}

bool pyth_add_symbol::has_status() const
{
  pyth_client *cptr = get_pyth_client();
  return cptr->has_status( PC_PYTH_RPC_CONNECTED |
                           PC_PYTH_HAS_BLOCK_HASH |
                           PC_PYTH_HAS_MAPPING );
}

void pyth_add_symbol::submit()
{
  pyth_client *cptr = get_pyth_client();
  // check if we already have this symbol
  if ( cptr->get_symbol_price( *sreq_->get_symbol() ) ) {
    on_error_sub( "symbol already exists", this );
    return;
  }
  key_pair *pkey = cptr->get_publish_key_pair();
  if ( !pkey ) {
    on_error_sub( "missing or invalid publish key [" +
        cptr->get_publish_key_pair_file() + "]", this );
    return;
  }
  key_pair *mkey = cptr->get_mapping_key_pair();
  if ( !mkey ) {
    on_error_sub( "missing or invalid mapping key [" +
        cptr->get_mapping_key_pair_file() + "]", this );
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
  areq_->set_space( sizeof( pc_price_node_t ) );
  sreq_->set_program( gpub );
  sreq_->set_publish( pkey );
  sreq_->set_account( &akey_ );
  sreq_->set_mapping( mkey );
  areq_->set_sub( this );
  sreq_->set_sub( this );
  sig_->set_sub( this );

  // get recent block hash and submit request
  st_ = e_create_sent;
  areq_->set_block_hash( get_pyth_client()->get_recent_block_hash() );
  get_rpc_client()->send( areq_ );
}

void pyth_add_symbol::on_response( rpc::create_account *res )
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

void pyth_add_symbol::on_response( rpc::signature_subscribe *res )
{
  if ( res->get_is_err() ) {
    on_error_sub( res->get_err_msg(), this );
    st_ = e_error;
  } else if ( st_ == e_create_sig ) {
    st_ = e_add_sent;
    sreq_->set_block_hash( get_pyth_client()->get_recent_block_hash() );
    get_rpc_client()->send( sreq_ );
  } else if ( st_ == e_add_sig ) {
    st_ = e_done;
    on_response_sub( this );
  }
}

void pyth_add_symbol::on_response( rpc::add_symbol *res )
{
  if ( res->get_is_err() ) {
    on_error_sub( res->get_err_msg(), this );
    st_ = e_error;
  } else if ( st_ == e_add_sent ) {
    st_ = e_add_sig;
    sig_->set_commitment( commitment::e_finalized );
    sig_->set_signature( res->get_signature() );
    get_rpc_client()->send( sig_ );
  }
}

bool pyth_add_symbol::get_is_done() const
{
  return st_ == e_done;
}

///////////////////////////////////////////////////////////////////////////
// pyth_add_publisher

void pyth_add_publisher::set_symbol( const symbol& sym )
{
  req_->set_symbol( sym );
}

void pyth_add_publisher::set_publisher( const pub_key& pub )
{
  req_->set_publisher( pub );
}

bool pyth_add_publisher::has_status() const
{
  pyth_client *cptr = get_pyth_client();
  return cptr->has_status( PC_PYTH_RPC_CONNECTED |
                           PC_PYTH_HAS_BLOCK_HASH |
                           PC_PYTH_HAS_MAPPING );
}

void pyth_add_publisher::submit()
{
  pyth_client *cptr = get_pyth_client();

  // check if this symbol exists
  pyth_symbol_price *sptr = cptr->get_symbol_price(
      *req_->get_symbol() );
  if ( !sptr ) {
    on_error_sub( "symbol does not exist", this );
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
  req_->set_block_hash( get_pyth_client()->get_recent_block_hash() );
  get_rpc_client()->send( req_ );
}

void pyth_add_publisher::on_response( rpc::add_publisher *res )
{
  if ( res->get_is_err() ) {
    on_error_sub( res->get_err_msg(), this );
    st_ = e_error;
  } else if ( st_ == e_add_sent ) {
    st_ = e_add_sig;
    sig_->set_commitment( commitment::e_finalized );
    sig_->set_signature( res->get_signature() );
    get_rpc_client()->send( sig_ );
  }
}

void pyth_add_publisher::on_response( rpc::signature_subscribe *res )
{
  if ( res->get_is_err() ) {
    on_error_sub( res->get_err_msg(), this );
    st_ = e_error;
  } else if ( st_ == e_add_sig ) {
    st_ = e_done;
    on_response_sub( this );
  }
}

bool pyth_add_publisher::get_is_done() const
{
  return st_ == e_done;
}

///////////////////////////////////////////////////////////////////////////
// pyth_symbol_price

pyth_symbol_price::pyth_symbol_price( const symbol& sym, const pub_key& acc )
: init_( false ),
  pxchg_( false ),
  st_( e_subscribe ),
  sym_( sym ),
  apub_( acc )
{
  __builtin_memset( &pnd_, 0, sizeof( pc_price_node_t ) );
  pc_symbol_assign( &pnd_.sym_, (pc_symbol_t*)sym.data() );
  areq_->set_account( &apub_ );
  sreq_->set_account( &apub_ );
  preq_->set_symbol( &sym_ );
  preq_->set_account( &apub_ );
  areq_->set_sub( this );
  sreq_->set_sub( this );
  preq_->set_sub( this );
}

bool pyth_symbol_price::init_publish()
{
  pyth_client *cptr = get_pyth_client();
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
  preq_->set_program( gpub );
  init_ = true;
  return true;
}

symbol *pyth_symbol_price::get_symbol()
{
  return &sym_;
}

pub_key *pyth_symbol_price::get_account()
{
  return &apub_;
}

void pyth_symbol_price::reset()
{
  st_ = e_subscribe;
  reset_err();
}

bool pyth_symbol_price::has_publisher( const pub_key& key )
{
  for(uint32_t i=0; i != pnd_.num_; ++i ) {
    pc_price_t *iptr = &pnd_.comp_[i];
    if ( pc_pub_key_equal( &iptr->pub_, (pc_pub_key_t*)key.data() ) ) {
      return true;
    }
  }
  return false;
}

void pyth_symbol_price::upd_price( int64_t price, uint64_t conf )
{
  preq_->set_price( price, conf );
  pyth_client *cptr = get_pyth_client();
  if ( cptr && st_ == e_publish ) {
    pxchg_ = false;
    cptr->submit( this );
  } else {
    pxchg_ = true;
  }
}

void pyth_symbol_price::submit()
{
  pyth_client *pptr = get_pyth_client();
  rpc_client  *cptr = get_rpc_client();
  if ( st_ == e_publish ) {
    if ( PC_UNLIKELY( get_is_err() ) ) {
      return;
    }
    if ( PC_UNLIKELY( !init_ && !init_publish() ) ) {
      return;
    }
    preq_->set_block_hash( pptr->get_recent_block_hash() );
    cptr->send( preq_ );
  } else if ( st_ == e_subscribe ) {
    cptr->send( areq_ );
    cptr->send( sreq_ );
    st_ = e_publish;
    // submit new price if one arrived while (re)subscribiing
    if ( pxchg_ ) {
      pxchg_ = false;
      pptr->submit( this );
    }
  }
}

void pyth_symbol_price::on_response( rpc::get_account_info *res )
{
  update( res );
}

void pyth_symbol_price::on_response( rpc::account_subscribe *res )
{
  update( res );
}

void pyth_symbol_price::on_response( rpc::upd_price *res )
{
  if ( res->get_is_err() ) {
    on_error_sub( res->get_err_msg(), this );
    st_ = e_error;
    return;
  }
  st_ = e_waitsig;
  sig_->set_commitment( commitment::e_confirmed );
  sig_->set_signature( res->get_signature() );
  get_rpc_client()->send( sig_ );
}

void pyth_symbol_price::on_response( rpc::signature_subscribe *res )
{
  if ( res->get_is_err() ) {
    on_error_sub( res->get_err_msg(), this );
    st_ = e_error;
  } else {
    st_ = e_publish;
    if ( pxchg_ ) {
      // submit next price if one submitted in meantime
      pyth_client *cptr = get_pyth_client();
      pxchg_ = false;
      cptr->submit( this );
    }
  }
}

template<class T>
void pyth_symbol_price::update( T *res )
{
  if ( res->get_is_err() ) {
    on_error_sub( res->get_err_msg(), this );
    st_ = e_error;
    return;
  }
  pc_price_node_t pupd[1];
  if ( !res->get_data( pupd ) ) {
    on_error_sub( "invalid symbol account size", this );
    st_ = e_error;
    return;
  }
  PC_LOG_INF( "symbol_update" )
    .add( "symbol", sym_ )
    .add( "account", apub_ )
    .add( "exponent", (int64_t)pupd->expo_ )
    .add( "num_pub", (int64_t)pupd->num_ )
    .end();
  uint32_t num = std::max( pnd_.num_, pupd->num_ );
  for( unsigned i=0; i != num; ++i ) {
    pc_price_t *pptr = &pnd_.comp_[i];
    const pc_price_t *nptr = &pupd->comp_[i];
    if ( !pc_pub_key_equal( &pptr->pub_, (pc_pub_key_t*)&nptr->pub_ ) ) {
      pc_pub_key_assign( &pptr->pub_, (pc_pub_key_t*)&nptr->pub_ );
      PC_LOG_INF( "symbol_publisher" )
        .add( "symbol", sym_ )
        .add( "pub_key", *(pub_key*)&pptr->pub_ )
        .end();
    }
    if ( pptr->price_ != nptr->price_ ) {
      pptr->price_ = nptr->price_;
      PC_LOG_INF( "symbol_price" )
        .add( "symbol", sym_ )
        .add( "pub_key", *(pub_key*)&pptr->pub_ )
        .add( "price", pptr->price_ )
        .end();
    }
    if ( pptr->conf_ != nptr->conf_ ) {
      pptr->conf_  = nptr->conf_;
      PC_LOG_INF( "symbol_confidence" )
        .add( "symbol", sym_ )
        .add( "pub_key", *(pub_key*)&pptr->pub_ )
        .add( "conf", pptr->conf_ )
        .end();
    }
  }
  on_response_sub( this );
}
