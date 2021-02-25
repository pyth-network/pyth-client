#include "jwriter.hpp"
#include "encode.hpp"

using namespace pc;

void jwriter::add_obj()
{
  if ( !first_ ) buf_[wtr_++] = ',';
  buf_[wtr_++] = '{';
  first_ = true;
  st_.push_back( e_obj );
}

void jwriter::add_arr()
{
  if ( !first_ ) buf_[wtr_++] = ',';
  buf_[wtr_++] = '[';
  first_ = true;
  st_.push_back( e_obj );
}

void jwriter::pop()
{
  buf_[wtr_++] = st_.back() == e_obj ? '}' : ']';
  first_ = false;
}

void jwriter::add_key_only( const char *key, size_t key_len )
{
  if ( first_ ) buf_[wtr_++] = ',';
  first_ = false;
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

void jwriter::add_val( const char *val, size_t val_len )
{
  if ( first_ ) buf_[wtr_++] = ',';
  first_ = false;
  buf_[wtr_++] = '"';
  __builtin_memcpy( &buf_[wtr_], val, val_len );
  wtr_ += val_len;
  buf_[wtr_++] = '"';
}

void jwriter::add_val( const char *val )
{
  add_val( val, __builtin_strlen( val ) );
}
