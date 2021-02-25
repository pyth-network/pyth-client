#include "rpc_client.hpp"

using namespace pc;

///////////////////////////////////////////////////////////////////////////
// rpc_client

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
//  rptr->serialize( msg );
// add_send( msg )
}

void rpc_client::parse_content( const char *txt, size_t len )
{
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

rpc_request::rpc_request()
: cb_( nullptr )
{
}

rpc_request::~rpc_request()
{
}

void rpc_request::set_rpc_sub( rpc_sub *cb )
{
  cb_ = cb;
}

rpc_sub *rpc_request::get_rpc_sub() const
{
  return cb_;
}
