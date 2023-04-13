#include <stdio.h>
#include "util.h"

int
test_sar() {

  prng_t _prng[1];
  prng_t * prng = prng_join( prng_new( _prng, (uint32_t)0, (uint64_t)0 ) );

  /* FIXME: EXPLICT COVERAGE OF EDGE CASES (PROBABLY STATICALLY FULLY
     SAMPLED ALREADY THOUGH FOR 8 AND 16 BIT TYPES) */

  for( int i=0; i<100000000; i++ ) {

    /* These tests assume the unit test platform has arithmetic right
       shift for signed integers (and the diagnostic printfs assume that
       long is at least a w-bit wide type) */

#   define TEST_SAR(w) do {                                                                                  \
      int##w##_t x   = (int##w##_t)prng_uint##w( prng );                                                     \
      int        n   = (int)( prng_uint32( prng ) & (uint32_t)(w-1) );                                       \
      int##w##_t val = sar_int##w( x, n );                                                                   \
      int##w##_t ref = (int##w##_t)(x >> n);                                                                 \
      if( val!=ref ) {                                                                                       \
        printf( "FAIL (iter %i op %i x %li n %i val %li ref %li)", i, w, (long)x, n, (long)val, (long)ref ); \
        return 1;                                                                                            \
      }                                                                                                      \
    } while(0)

    TEST_SAR(8);
    TEST_SAR(16);
    TEST_SAR(32);
    TEST_SAR(64);
#   undef TEST_SAR

  }

  prng_delete( prng_leave( prng ) );

  return 0;
}
