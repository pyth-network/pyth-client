#include "rpc_client.hpp"
#include <iostream>

using namespace pc;

///////////////////////////////////////////////////////////////////////////
// rpc_client

rpc_client::rpc_client()
: id_( 0UL )
{
}

void rpc_client::send( rpc_request *rptr )
{
  // get request id
  uint64_t id;
  if ( !rd_.empty() ) {
    id = rd_.back();
    rd_.pop_back();
  } else {
    id = ++id_;
    rv_.resize( 1 + id, nullptr );
  }
  rv_[id] = rptr;

  // construct json message
  jw_.attach( jb_ );
  jw_.add_val( jwriter::e_obj );
  jw_.add_key( "jsonrpc", "2.0" );
  jw_.add_key( "id", id );
  rptr->serialize( jw_ );
  jw_.pop();

  std::cout.write( jb_, jw_.size() );
  std::cout << std::endl;

  // submit http POST request
  http_request msg;
  msg.init( "POST", "/" );
  msg.add_hdr( "Content-Type", "application/json" );
  net_wtr bdy;
  bdy.add( jb_, jw_.size() );
  msg.add_content( bdy );
  add_send( msg );
}

void rpc_client::parse_content( const char *txt, size_t len )
{
  std::cout.write( txt, len );
  std::cout << std::endl;

  // parse and redirect response to corresponding request
  jp_.parse( txt, len );
  uint32_t idtok = jp_.find_val( 1, "id" );
  if ( idtok ) {
    uint64_t id = jp_.get_uint( idtok );
    if ( id < rv_.size() ) {
      rpc_request *rptr = rv_[id];
      if ( rptr ) {
        rptr->deserialize( jp_ );
        rv_[id] = nullptr;
        rd_.push_back( id );
      }
    }
  }
}

///////////////////////////////////////////////////////////////////////////
// rpc_request

rpc_sub::~rpc_sub()
{
}

rpc_request::rpc_request()
: cb_( nullptr )
{
}

rpc_request::~rpc_request()
{
}

void rpc_request::set_sub( rpc_sub *cb )
{
  cb_ = cb;
}

rpc_sub *rpc_request::get_sub() const
{
  return cb_;
}

///////////////////////////////////////////////////////////////////////////
// get_account_info

void rpc::get_account_info::set_account( const pub_key& acc )
{
  acc_ = acc;
}

uint64_t rpc::get_account_info::get_slot() const
{
  return slot_;
}

bool rpc::get_account_info::get_is_executable() const
{
  return is_exec_;
}

uint64_t rpc::get_account_info::get_lamports() const
{
  return lamports_;
}

uint64_t rpc::get_account_info::get_rent_epoch() const
{
  return rent_epoch_;
}

void rpc::get_account_info::get_owner( const char *&optr, size_t& olen) const
{
  optr = optr_;
  olen = olen_;
}

void rpc::get_account_info::get_data( const char *&dptr, size_t& dlen) const
{
  dptr = dptr_;
  dlen = dlen_;
}

rpc::get_account_info::get_account_info()
: slot_(0),
  lamports_( 0 ),
  rent_epoch_( 0 ),
  dptr_( nullptr ),
  dlen_( 0 ),
  optr_( nullptr ),
  olen_( 0 ),
  is_exec_( false )
{
}

void rpc::get_account_info::serialize( jwriter& msg )
{
  msg.add_key( "method", "getAccountInfo" );
  msg.add_key( "params", jwriter::e_arr );
  msg.add_val( acc_ );
  msg.pop();
}

void rpc::get_account_info::deserialize( const jtree& jt )
{
  uint32_t rtok = jt.find_val( 1, "result" );
  uint32_t ctok = jt.find_val( rtok, "context" );
  slot_ = jt.get_uint( jt.find_val( ctok, "slot" ) );
  uint32_t vtok = jt.find_val( rtok, "value" );
  is_exec_ = jt.get_bool( jt.find_val( vtok, "executable" ) );
  lamports_ = jt.get_uint( jt.find_val( vtok, "lamports" ) );
  jt.get_text( jt.find_val( vtok, "data" ), dptr_, dlen_ );
  jt.get_text( jt.find_val( vtok, "owner" ), optr_, olen_ );
  rent_epoch_ = jt.get_uint( jt.find_val( vtok, "rentEpoch" ) );
  on_response( this );
}
