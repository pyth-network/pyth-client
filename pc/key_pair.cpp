#include "key_pair.hpp"
#include "jtree.hpp"
#include "mem_map.hpp"
#include "misc.hpp"
#include <openssl/evp.h>

#include <assert.h>

#define PC_SYMBOL_SPACES 0x2020202020202020

using namespace pc;

hash::hash()
{
}

hash::hash( const hash& obj )
{
  *this = obj;
}

hash& hash::operator=( const hash& obj )
{
  i_[0] = obj.i_[0];
  i_[1] = obj.i_[1];
  i_[2] = obj.i_[2];
  i_[3] = obj.i_[3];
  return *this;
}

bool hash::operator==( const hash& obj) const
{
  return i_[0] == obj.i_[0] &&
    i_[1] == obj.i_[1] &&
    i_[2] == obj.i_[2] &&
    i_[3] == obj.i_[3];
}

bool hash::operator!=( const hash& obj) const
{
  return i_[0] != obj.i_[0] ||
    i_[1] != obj.i_[1] ||
    i_[2] != obj.i_[2] ||
    i_[3] != obj.i_[3];
}

void hash::zero()
{
  i_[0] = i_[1] = i_[2] = i_[3] = 0UL;
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

bool hash::init_from_text( str buf )
{
  pc::dec_base58( (const uint8_t*)buf.str_, buf.len_, pk_ );
  return true;
}

void hash::init_from_buf( const uint8_t *pk )
{
  __builtin_memcpy( pk_, pk, len );
}

int hash::enc_base58( char *buf, int buflen ) const
{
  return pc::enc_base58( pk_, len, buf, buflen );
}

int hash::enc_base58( std::string& res ) const
{
  char buf[64];
  int n = enc_base58( buf, 64 );
  assert( n >= 0 );
  res.assign( buf, static_cast< unsigned >( n ) );
  return n;
}

int hash::dec_base58( const uint8_t *buf, int buflen )
{
  return pc::dec_base58( buf, buflen, pk_ );
}

pub_key::pub_key()
{
}

pub_key::pub_key( const pub_key& obj )
: hash( obj )
{
}

pub_key::pub_key( const key_pair& kp )
{
  kp.get_pub_key( *this );
}

pub_key& pub_key::operator=( const pub_key& pk )
{
  return (pub_key&)hash::operator=( pk );
}

void key_pair::gen()
{
  EVP_PKEY *pkey = NULL;
  EVP_PKEY_CTX *pctx = EVP_PKEY_CTX_new_id(EVP_PKEY_ED25519, NULL);
  EVP_PKEY_keygen_init(pctx);
  EVP_PKEY_keygen(pctx, &pkey);
  EVP_PKEY_CTX_free(pctx);
  size_t len[] = { pub_key::len };
  EVP_PKEY_get_raw_private_key( pkey, pk_, len );
  EVP_PKEY_get_raw_public_key( pkey, &pk_[pub_key::len], len );
  EVP_PKEY_free( pkey );
}

void key_pair::zero()
{
  __builtin_memset( pk_, 0, len );
}

bool key_pair::init_from_file( const std::string& file )
{
  mem_map mp;
  mp.set_file( file );
  if ( !mp.init() ) {
    return false;
  }
  return init_from_json( mp.data(), mp.size() );
}

bool key_pair::init_from_json( const std::string& buf )
{
  return init_from_json( buf.c_str(), buf.length() );
}

bool key_pair::init_from_json( const char *buf, size_t len )
{
  jtree jt;
  jt.parse( buf, len );
  uint8_t *pk = pk_;
  for( uint32_t it = jt.get_first(1); it; it = jt.get_next( it ) ) {
    *pk++ = (uint8_t)jt.get_uint( it );
  }
  return key_pair::len == (size_t)(pk - pk_);
}

void key_pair::get_pub_key( pub_key& pk ) const
{
  pk.init_from_buf( &pk_[pub_key::len] );
}

key_cache::key_cache()
: ptr_( nullptr )
{
}

key_cache::~key_cache()
{
  if ( ptr_ ) {
    EVP_PKEY_free( (EVP_PKEY*)ptr_ );
    ptr_ = nullptr;
  }
}

void key_cache::set( const key_pair& pk )
{
  EVP_PKEY *pkey = EVP_PKEY_new_raw_private_key(
      EVP_PKEY_ED25519, NULL, pk.data(), pub_key::len );
  ptr_ = (void*)pkey;
}

void *key_cache::get() const
{
  return ptr_;
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

int signature::enc_base58( char *buf, int buflen )
{
  return pc::enc_base58( sig_, len, buf, buflen );
}

int signature::enc_base58( std::string& res )
{
  char buf[256];
  int n = enc_base58( buf, 256 );
  assert( n >= 0 );
  res.assign( buf, static_cast< unsigned >( n ) );
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

bool signature::sign(
    const uint8_t* msg, uint32_t msg_len, const key_cache& kp )
{
  EVP_PKEY *pkey = (EVP_PKEY*)kp.get();
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
