#pragma once

#include <pc/key_pair.hpp>
#include <stdint.h>

namespace pc
{

  // binary serialization/deserialization in the
  // spirit of rust bincode convention
  class bincode
  {
  public:
    bincode();
    bincode( char* );

    // attach buffer
    void attach( char *buf );
    char *get_buf() const;
    char *get_wtr() const;
    size_t size() const;

    // get/modify current read/write position
    size_t get_pos() const;
    void set_pos( size_t );
    void reset_pos();

    // reserve slot for signature
    size_t reserve_sign();

    // sign message at position sig for message starting at position msg
    void sign( size_t sig, size_t msg, const key_pair& );

    // add values to buffer
    void add( uint8_t );
    void add( uint32_t );
    void add( uint64_t );
    void add( int32_t );
    void add( int64_t );
    void add( const pub_key& );
    void add( const hash& );
    void add( const char *, size_t );

    // add (fixed) array length encoding
    template<unsigned N> void add_len();
    void add_len( unsigned );

  private:
    template<class T> void add_val_T( T val );
    template<class T> void add_ref_T( const T& val );
    char  *buf_;
    size_t idx_;
  };

  inline bincode::bincode()
  : buf_( nullptr ), idx_( 0 ) {
  }

  inline bincode::bincode( char *buf )
  : buf_( buf ), idx_( 0 ) {
  }

  inline void bincode::attach( char *buf )
  {
    buf_ = buf;
    idx_ = 0;
  }

  inline char *bincode::get_buf() const
  {
    return buf_;
  }

  inline char *bincode::get_wtr() const
  {
    return &buf_[idx_];
  }

  inline size_t bincode::size() const
  {
    return idx_;
  }

  inline void bincode::reset_pos()
  {
    idx_ = 0;
  }

  inline size_t bincode::get_pos() const
  {
    return idx_;
  }

  inline void bincode::set_pos( size_t pos )
  {
    idx_ = pos;
  }

  inline void bincode::add( uint8_t val )
  {
    buf_[idx_++] = val;
  }

  template<class T>
  void bincode::add_val_T( T val )
  {
    T *p = (T*)&buf_[idx_];
    p[0] = val;
    idx_ += sizeof( T );
  }

  template<class T>
  void bincode::add_ref_T( const T& val )
  {
    T *p = (T*)&buf_[idx_];
    p[0] = val;
    idx_ += sizeof( T );
  }

  inline void bincode::add( uint32_t val )
  {
    add_val_T( val );
  }

  inline void bincode::add( uint64_t val )
  {
    add_val_T( val );
  }

  inline void bincode::add( int32_t val )
  {
    add_val_T( val );
  }

  inline void bincode::add( int64_t val )
  {
    add_val_T( val );
  }

  inline void bincode::add( const pub_key& pk )
  {
    add( (const hash&)pk );
  }

  inline void bincode::add( const hash& pk )
  {
    __builtin_memcpy( &buf_[idx_], pk.data(), hash::len );
    idx_ += hash::len;
  }

  inline void bincode::add( const char *buf, size_t len )
  {
    __builtin_memcpy( &buf_[idx_], buf, len );
    idx_ += len;
  }

  template<unsigned N> void bincode::add_len()
  {
    if ( N < 0x80 ) {
      buf_[idx_++] = (char)(N&0x7f);
    } else {
      buf_[idx_++] = (char)(0x80 | (N&0x7f) );
      buf_[idx_++] = (char)((N>>7)&0x7f);
    }
  }

  inline void bincode::add_len( unsigned N )
  {
    if ( N < 0x80 ) {
      buf_[idx_++] = (char)(N&0x7f);
    } else {
      buf_[idx_++] = (char)(0x80 | (N&0x7f) );
      buf_[idx_++] = (char)((N>>7)&0x7f);
    }
  }

  inline size_t bincode::reserve_sign()
  {
    size_t idx = idx_;
    idx_ += signature::len;
    return idx;
  }

  inline void bincode::sign( size_t sig, size_t msg, const key_pair& kp )
  {
    signature *sptr = (signature*)&buf_[sig];
    sptr->sign( (const uint8_t*)&buf_[msg], idx_ - msg, kp );
  }

}
