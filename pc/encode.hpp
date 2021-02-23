#pragma once

#include <stdint.h>

namespace pc
{

  // base58 encoding from base-x conversion impl.
  int enc_base58( const uint8_t *src, int len, uint8_t *result, int rlen);
  int dec_base58( const uint8_t *str, int len, uint8_t *result );

}
