#pragma once

#include <stdint.h>
#include <string>

#define PC_PACKED        __attribute__((__packed__))
#define PC_UNLIKELY(ARG) __builtin_expect((ARG),1)
#define PC_NSECS_IN_SEC  1000000000L

namespace pc
{

  // base58 encoding from base-x conversion impl.
  int enc_base58( const uint8_t *src, int len, uint8_t *result, int rlen);
  int dec_base58( const uint8_t *str, int len, uint8_t *result );

  // base64 encoding courtesy of
  // Adam Rudd per licence: github.com/adamvr/arduino-base64
  int enc_base64_len( int len );
  int enc_base64( const uint8_t *src, int len, uint8_t *result );
  int dec_base64( const uint8_t *str, int len, uint8_t *result );

  // integer to string encoding
  char *uint_to_str( uint64_t val, char *end_ptr );
  uint64_t str_to_uint( const char *str, int len );
  char *int_to_str( int64_t val, char *end_ptr );
  int64_t str_to_int( const char *str, int len );

  // string representation of decimal to integer with implied decimal places
  int64_t str_to_dec( const char *str, int len, int expo );
  int64_t str_to_dec( const char *str, int expo );

  // current time
  int64_t get_now();
  char *nsecs_to_utc6( int64_t ts, char *cptr );

  // string as char pointer plus length
  struct str
  {
    str( const char * );
    str( const char *, size_t );
    str( const uint8_t *, size_t );
    str( const std::string& );
    bool operator==( const str& ) const;
    const char *str_;
    size_t      len_;
  };

  /////////////////////////////////////////////////////////////////////////
  // inline impl

  inline str::str( const char *str )
  : str_( str ), len_( __builtin_strlen( str ) ) {
  }

  inline str::str( const char *str, size_t len )
  : str_( str ), len_( len ) {
  }

  inline str::str( const uint8_t *str, size_t len )
  : str_( (const char*)str ), len_( len ) {
  }

  inline str::str( const std::string& str )
  : str_( str.c_str() ), len_( str.length() ) {
  }

  inline bool str::operator==( const str& obj ) const
  {
    return len_ == obj.len_ &&
           0 == __builtin_strncmp( str_, obj.str_, len_);
  }

}
