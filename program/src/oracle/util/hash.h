#ifndef _pyth_oracle_util_hash_h_
#define _pyth_oracle_util_hash_h_

/* Useful hashing utilities */

#include "compat_stdint.h"

#ifdef __cplusplus
extern "C" {
#endif

/* High quality (full avalanche) high speed integer to integer hashing.
   hash_uint32 has the properties that [0,2^32) hashes to a random
   looking permutation of [0,2^32) and hash(0)==0.  Similarly for
   hash_uint64.  Based on Google's Murmur3 hash finalizer (public
   domain).  Not cryptographically secure but passes various strict
   tests of randomness when used as a PRNG (see prng.h). */

static inline uint32_t
hash_uint32( uint32_t x ) {
  x ^= x >> 16;
  x *= UINT32_C( 0x85ebca6b );
  x ^= x >> 13;
  x *= UINT32_C( 0xc2b2ae35 );
  x ^= x >> 16;
  return x;
}

static inline uint64_t
hash_uint64( uint64_t x ) {
  x ^= x >> 33;
  x *= UINT64_C( 0xff51afd7ed558ccd );
  x ^= x >> 33;
  x *= UINT64_C( 0xc4ceb9fe1a85ec53 );
  x ^= x >> 33;
  return x;
}

/* Inverses of the above.  E.g.:
     hash_inverse_uint32( hash_uint32( (uint32_t)x ) )==(uint32_t)x
   and:
     hash_uint32( hash_inverse_uint32( (uint32_t)x ) )==(uint32_t)x
   Similarly for hash_inverse_uint64.  These by themselves are similar
   quality hashes to the above and having the inverses of the above can
   be useful.  The fact these have (nearly) identical operations /
   operation counts concretely demonstrates that none of these are
   standalone cryptographically secure. */

static inline uint32_t
hash_inverse_uint32( uint32_t x ) {
  x ^= x >> 16;
  x *= UINT32_C( 0x7ed1b41d );
  x ^= (x >> 13) ^ (x >> 26);
  x *= UINT32_C( 0xa5cb9243 );
  x ^= x >> 16;
  return x;
}

static inline uint64_t
hash_inverse_uint64( uint64_t x ) {
  x ^= x >> 33;
  x *= UINT64_C( 0x9cb4b2f8129337db );
  x ^= x >> 33;
  x *= UINT64_C( 0x4f74430c22a54005 );
  x ^= x >> 33;
  return x;
}

#ifdef __cplusplus
}
#endif

#endif /* _pyth_oracle_util_hash_h_ */

