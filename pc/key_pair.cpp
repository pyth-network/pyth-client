#include "key_pair.hpp"
#include "json.hpp"
#include "mem_map.hpp"
#include "encode.hpp"
#include <openssl/evp.h>

namespace pc
{
  struct key_handler : public json::handler
  {
    key_handler( uint8_t *pk ) : pk_( pk ), idx_( 0 ) {
    }
    void parse_number( const char *ptr, const char *end ) {
      char *eptr[1] = { (char*)end };
      pk_[idx_++] = (uint8_t)strtol( ptr, eptr, 10 );
    }
    uint8_t *pk_;
    uint32_t idx_;
  };
}

using namespace pc;

hash::hash()
{
}

void hash::zero()
{
  __builtin_memset( pk_, 0, len );
}

bool hash::init_from_file( const std::string& file )
{
  mem_map mp;
  mp.set_file( file );
  if ( !mp.init() ) {
    return false;
  }
  pc::dec_base58( (const uint8_t*)mp.data(), mp.size(), pk_ );
  return true;
}

bool hash::init_from_text( const std::string& buf )
{
  pc::dec_base58( (const uint8_t*)buf.c_str(), buf.length(), pk_ );
  return true;
}

void hash::init_from_buf( const uint8_t *pk )
{
  __builtin_memcpy( pk_, pk, len );
}

int hash::enc_base58( uint8_t *buf, uint32_t buflen )
{
  return pc::enc_base58( pk_, len, buf, buflen );
}

int hash::enc_base58( std::string& res )
{
  char buf[64];
  int n = enc_base58( (uint8_t*)buf, 64 );
  res.assign( buf, n );
  return n;
}

int hash::dec_base58( const uint8_t *buf, uint32_t buflen )
{
  return pc::dec_base58( buf, buflen, pk_ );
}

pub_key::pub_key()
{
}

pub_key::pub_key( const key_pair& kp )
{
  kp.get_pub_key( *this );
}

bool key_pair::init_from_file( const std::string& file )
{
  mem_map mp;
  mp.set_file( file );
  if ( !mp.init() ) {
    return false;
  }
  key_handler kh( pk_ );
  json::parse( mp.data(), mp.size(), &kh );
  return true;
}

bool key_pair::init_from_json( const std::string& buf )
{
  key_handler kh( pk_ );
  json::parse( buf.c_str(), buf.length(), &kh );
  return true;
}

void key_pair::get_pub_key( pub_key& pk ) const
{
  pk.init_from_buf( &pk_[pub_key::len] );
}

void signature::init_from_buf( const uint8_t *buf )
{
  __builtin_memcpy( sig_, buf, len );
}

bool signature::init_from_text( const std::string& buf )
{
  pc::dec_base58( (const uint8_t*)buf.c_str(), buf.length(), sig_ );
  return true;
}

int signature::enc_base58( uint8_t *buf, uint32_t buflen )
{
  return pc::enc_base58( sig_, len, buf, buflen );
}

int signature::enc_base58( std::string& res )
{
  char buf[256];
  int n = enc_base58( (uint8_t*)buf, 256 );
  res.assign( buf, n );
  return n;
}

bool signature::sign(
    const uint8_t* msg, uint32_t msg_len, const key_pair& kp )
{
  EVP_PKEY *pkey = EVP_PKEY_new_raw_private_key( EVP_PKEY_ED25519,
      NULL, kp.data(), pub_key::len );
  if ( !pkey ) {
    return false;
  }
  EVP_MD_CTX *mctx = EVP_MD_CTX_new();
  int rc = EVP_DigestSignInit( mctx, NULL, NULL, NULL, pkey );
  if ( rc ) {
    size_t sig_len[1] = { len };
    rc = EVP_DigestSign( mctx, sig_, sig_len, msg, msg_len );
  }
  EVP_MD_CTX_free( mctx );
  EVP_PKEY_free( pkey );
  return rc != 0;
}

bool signature::verify(
    const uint8_t* msg, uint32_t msg_len, const pub_key& pk )
{
  EVP_PKEY *pkey = EVP_PKEY_new_raw_public_key( EVP_PKEY_ED25519,
      NULL, pk.data(), pub_key::len );
  if ( !pkey ) {
    return false;
  }
  EVP_MD_CTX *mctx = EVP_MD_CTX_new();
  int rc = EVP_DigestSignInit( mctx, NULL, NULL, NULL, pkey );
  if ( rc ) {
    rc = EVP_DigestVerify( mctx, sig_, len, msg, msg_len );
  }
  EVP_MD_CTX_free( mctx );
  EVP_PKEY_free( pkey );
  return rc != 0;
}
