#pragma once

#include <stdint.h>

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

}
