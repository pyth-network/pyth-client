#include "rpc_client.hpp"
#include "bincode.hpp"
#include <iostream>

using namespace pc;

// system program instructions
enum system_instruction : uint32_t
{
  e_create_account,
  e_assign,
  e_transfer
};

static hash gen_sys_id()
{
  pc::hash id;
  id.zero();
  return id;
}

// system program id
static hash sys_id = gen_sys_id();

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
  msg.add_content( jb_, jw_.size() );
  add_send( msg );
}

void rpc_client::parse_content( const char *txt, size_t len )
{
  std::cout.write( txt, len );
  std::cout << std::endl;

  // parse and redirect response to corresponding request
  jp_.parse( txt, len );
  uint32_t ertok = jp_.find_val( 1, "error" );
  if ( ertok ) {
    // TODO: process error
  }
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

///////////////////////////////////////////////////////////////////////////
// get_recent_block_hash

uint64_t rpc::get_recent_block_hash::get_slot() const
{
  return slot_;
}

hash rpc::get_recent_block_hash::get_block_hash() const
{
  return bhash_;
}

uint64_t rpc::get_recent_block_hash::get_lamports_per_signature() const
{
  return fee_per_sig_;
}

rpc::get_recent_block_hash::get_recent_block_hash()
: slot_( 0 ),
  fee_per_sig_( 0 )
{
  bhash_.zero();
}

void rpc::get_recent_block_hash::serialize( jwriter& msg )
{
  msg.add_key( "method", "getRecentBlockhash" );
}

void rpc::get_recent_block_hash::deserialize( const jtree& jt )
{
  uint32_t rtok = jt.find_val( 1, "result" );
  uint32_t ctok = jt.find_val( rtok, "context" );
  slot_ = jt.get_uint( jt.find_val( ctok, "slot" ) );
  uint32_t vtok = jt.find_val( rtok, "value" );
  size_t txt_len = 0;
  const char *txt;
  jt.get_text( jt.find_val( vtok, "blockhash" ), txt, txt_len );
  bhash_.dec_base58( (const uint8_t*)txt, txt_len );
  uint32_t ftok = jt.find_val( vtok, "feeCalculator" );
  fee_per_sig_ = jt.get_uint( jt.find_val( ftok, "lamportsPerSignature" ) );
  on_response( this );
}

///////////////////////////////////////////////////////////////////////////
// transfer

void rpc::transfer::set_block_hash( const hash& bhash )
{
  bhash_ = bhash;
}

void rpc::transfer::set_sender( const key_pair& snd )
{
  snd_ = snd;
}

void rpc::transfer::set_receiver( const pub_key& rcv )
{
  rcv_ = rcv;
}

void rpc::transfer::set_lamports( uint64_t funds )
{
  lamports_ = funds;
}

rpc::transfer::transfer()
: lamports_( 0UL )
{
}

void rpc::transfer::serialize( jwriter& msg )
{
  // construct binary transaction
  net_buf *bptr = net_buf::alloc();
  bincode tx( bptr->buf_ );

  // signatures section
  tx.add_len<1>();      // one signature
  size_t sign_idx = tx.reserve_sign();

  // message header
  size_t tx_idx = tx.get_pos();
  tx.add( (uint8_t)1 ); // signing accounts
  tx.add( (uint8_t)0 ); // read-only signed accounts
  tx.add( (uint8_t)1 ); // read-only unsigned accounts

  // accounts
  tx.add_len<3>();      // 3 accounts: sender, receiver, program
  tx.add( snd_ );       // sender account
  tx.add( rcv_ );       // receiver account
  tx.add( sys_id );     // system programid

  // recent block hash
  tx.add( bhash_ );     // recent block hash

  // instructions section
  tx.add_len<1>();      // one instruction
  tx.add( (uint8_t)2);  // program_id index
  tx.add_len<2>();      // 2 accounts: sender, receiver
  tx.add( (uint8_t)0 ); // index of sender account
  tx.add( (uint8_t)1 ); // index of receiver account

  // instruction parameter section
  tx.add_len<12>();     // size of data array
  tx.add( (uint32_t)system_instruction::e_transfer );
  tx.add( (uint64_t)lamports_ );

  // sign message
  tx.sign( sign_idx, tx_idx, snd_ );

  // encode transaction and add to json params
  msg.add_key( "method", "sendTransaction" );
  msg.add_key( "params", jwriter::e_arr );
  msg.add_val_enc_base64( (const uint8_t*)tx.get_buf(), tx.size() );
  msg.add_val( jwriter::e_obj );
  msg.add_key( "encoding", "base64" );
  msg.pop();
  msg.pop();
  bptr->dealloc();
}

void rpc::transfer::deserialize( const jtree& )
{
  on_response( this );
}
