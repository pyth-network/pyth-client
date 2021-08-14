#include "tx_rpc_client.hpp"
#include <pc/bincode.hpp>

using namespace pc;

///////////////////////////////////////////////////////////////////////////
// rpc_request

template<class T>
void rpc_request::on_response( T *req )
{
  req->set_recv_time( get_now() );
  rpc_sub_i<T> *iptr = dynamic_cast<rpc_sub_i<T>*>( req->get_sub() );
  if ( iptr ) {
    iptr->on_response( req );
  }
}

template<class T>
bool rpc_request::on_error( const jtree& jt, T *req )
{
  uint32_t etok = jt.find_val( 1, "error" );
  if ( etok == 0 ) return false;
  const char *txt = nullptr;
  size_t txt_len = 0;
  std::string emsg;
  jt.get_text( jt.find_val( etok, "message" ), txt, txt_len );
  emsg.assign( txt, txt_len );
  set_err_msg( emsg );
  set_err_code( jt.get_int( jt.find_val( etok, "code" ) ) );
  on_response( req );
  return true;
}


///////////////////////////////////////////////////////////////////////////
// get_health

void rpc::get_health::request( json_wtr& msg )
{
  msg.add_key( "method", "getHealth" );
}

void rpc::get_health::response( const jtree& jt )
{
  if ( on_error( jt, this ) ) return;
  on_response( this );
}

///////////////////////////////////////////////////////////////////////////
// get_cluster_nodes

bool rpc::get_cluster_nodes::get_ip_addr( const pub_key& pkey, ip_addr& res )
{
  node_map_t::iter_t it = nmap_.find( pkey );
  if ( it ) {
    res = nmap_.obj( it );
    return true;
  } else {
    return false;
  }
}

void rpc::get_cluster_nodes::request( json_wtr& msg )
{
  msg.add_key( "method", "getClusterNodes" );
}

void rpc::get_cluster_nodes::response( const jtree& jt )
{
  if ( on_error( jt, this ) ) return;
  uint32_t rtok = jt.find_val( 1, "result" );
  pub_key pkey;
  for( uint32_t tok = jt.get_first( rtok ); tok; tok = jt.get_next( tok ) ) {
    pkey.init_from_text( jt.get_str( jt.find_val( tok, "pubkey" ) ) );
    ip_addr addr( jt.get_str( jt.find_val( tok, "tpu" ) ) );
    node_map_t::iter_t it = nmap_.find( pkey );
    if ( !it ) it = nmap_.add( pkey );
    nmap_.ref( it ) = addr;
  }
  on_response( this );
}

///////////////////////////////////////////////////////////////////////////
// get_slot_leaders

rpc::get_slot_leaders::get_slot_leaders()
: rslot_( 0UL ),
  limit_( 0UL ),
  lslot_( 0UL )
{
}

void rpc::get_slot_leaders::set_slot(uint64_t slot)
{
  rslot_ = slot;
}

void rpc::get_slot_leaders::set_limit( uint64_t limit )
{
  limit_ = limit;
}

uint64_t rpc::get_slot_leaders::get_last_slot() const
{
  return lslot_ + limit_;
}

pub_key *rpc::get_slot_leaders::get_leader( uint64_t slot )
{
  uint64_t idx = slot - lslot_;
  if ( idx < lvec_.size() ) {
    return &lvec_[idx];
  } else {
    return nullptr;
  }
}

void rpc::get_slot_leaders::request( json_wtr& msg )
{
  msg.add_key( "method", "getSlotLeaders" );
  msg.add_key( "params", json_wtr::e_arr );
  msg.add_val( rslot_ );
  msg.add_val( limit_ );
  msg.pop();
}

void rpc::get_slot_leaders::response( const jtree& jt )
{
  lvec_.clear();
  lslot_ = rslot_;
  if ( on_error( jt, this ) ) return;
  uint32_t rtok = jt.find_val( 1, "result" );
  pub_key pkey;
  for( uint32_t tok = jt.get_first( rtok ); tok; tok = jt.get_next( tok ) ) {
    pkey.init_from_text( jt.get_str( tok ) );
    lvec_.push_back( pkey );
  }
  on_response( this );
}

///////////////////////////////////////////////////////////////////////////
// slot_subscribe

uint64_t rpc::slot_subscribe::get_slot() const
{
  return slot_;
}

void rpc::slot_subscribe::request( json_wtr& msg )
{
  msg.add_key( "method", "slotSubscribe" );
}

void rpc::slot_subscribe::response( const jtree& jt )
{
  if ( on_error( jt, this ) ) return;
  // add to notification list
  add_notify( jt );
}

bool rpc::slot_subscribe::notify( const jtree& jt )
{
  if ( on_error( jt, this ) ) return true;
  uint32_t ptok = jt.find_val( 1, "params" );
  uint32_t rtok = jt.find_val( ptok, "result" );
  slot_ = jt.get_uint( jt.find_val( rtok, "slot" ) );
  on_response( this );
  return false; // keep notification
}
