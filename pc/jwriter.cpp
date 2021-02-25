#include "jwriter.hpp"
#include "encode.hpp"

using namespace pc;

jwriter::jwriter()
: first_( true )
{
}

jwriter::jwriter( char *buf )
{
  attach( buf );
}

void jwriter::attach( char *buf )
{
  st_.clear();
  wtr_ = 0;
  buf_ = buf;
  first_ = true;
}

void jwriter::add_obj()
{
  buf_[wtr_++] = '{';
  first_ = true;
  st_.push_back( e_obj );
}

void jwriter::add_arr()
{
  buf_[wtr_++] = '[';
  first_ = true;
  st_.push_back( e_arr );
}

void jwriter::pop()
{
  buf_[wtr_++] = st_.back() == e_obj ? '}' : ']';
  st_.pop_back();
  first_ = false;
}

void jwriter::add_first()
{
  if ( !first_ ) buf_[wtr_++] = ',';
  first_ = false;
}

void jwriter::add_key_only( const char *key, size_t key_len )
{
  add_first();
  buf_[wtr_++] = '"';
  __builtin_memcpy( &buf_[wtr_], key, key_len );
  wtr_ += key_len;
  buf_[wtr_++] = '"';
  buf_[wtr_++] = ':';
}

void jwriter::add_key( const char *key, size_t key_len,
                       const char *val, size_t val_len )
{
  add_key_only( key, key_len );
  buf_[wtr_++] = '"';
  __builtin_memcpy( &buf_[wtr_], val, val_len );
  wtr_ += val_len;
  buf_[wtr_++] = '"';
}

void jwriter::add_key( const char *key, const char *val, size_t val_len )
{
  add_key( key, __builtin_strlen( key ), val, val_len );
}

void jwriter::add_key( const char *key, const char *val )
{
  add_key( key, __builtin_strlen( key ), val, __builtin_strlen( val) );
}

void jwriter::add_key( const char *key, uint64_t ival )
{
  add_key_only( key, __builtin_strlen(key) );
  char buf[32], *end=&buf[31];
  char *val = uint_to_str( ival, end );
  size_t val_len = end - val;
  __builtin_memcpy( &buf_[wtr_], val, val_len );
  wtr_ += val_len;
}

void jwriter::add_key( const char *key, type_t t )
{
  add_key_only( key, __builtin_strlen(key) );
  if ( t == e_obj ) {
    add_obj();
  } else {
    add_arr();
  }
}

void jwriter::add_key( const char *key, const hash& pk )
{
  add_key_only( key, __builtin_strlen(key) );
  buf_[wtr_++] = '"';
  wtr_ += enc_base58( pk.data(), hash::len,
      (uint8_t*)&buf_[wtr_], 3*hash::len );
  buf_[wtr_++] = '"';
}

void jwriter::add_val( const char *val, size_t val_len )
{
  add_first();
  buf_[wtr_++] = '"';
  __builtin_memcpy( &buf_[wtr_], val, val_len );
  wtr_ += val_len;
  buf_[wtr_++] = '"';
}

void jwriter::add_val( const char *val )
{
  add_val( val, __builtin_strlen( val ) );
}

void jwriter::add_val( const hash& pk )
{
  add_val_enc_base58( pk.data(), hash::len );
}

void jwriter::add_val( const signature& sig )
{
  add_val_enc_base58( sig.data(), signature::len );
}

void jwriter::add_val( uint64_t ival )
{
  add_first();
  char buf[32], *end=&buf[31];
  char *val = uint_to_str( ival, end );
  size_t val_len = end - val;
  __builtin_memcpy( &buf_[wtr_], val, val_len );
  wtr_ += val_len;
}

void jwriter::add_val( type_t t )
{
  add_first();
  if ( t == e_obj ) {
    add_obj();
  } else {
    add_arr();
  }
}

void jwriter::add_val_enc_base58( const uint8_t *val, size_t val_len )
{
  add_first();
  buf_[wtr_++] = '"';
  wtr_ += enc_base58( val, val_len,
      (uint8_t*)&buf_[wtr_], 3*val_len );
  buf_[wtr_++] = '"';
}

void jwriter::add_val_enc_base64( const uint8_t *val, size_t val_len )
{
  add_first();
  buf_[wtr_++] = '"';
  wtr_ += enc_base64( val, val_len, (uint8_t*)&buf_[wtr_] );
  buf_[wtr_++] = '"';
}
