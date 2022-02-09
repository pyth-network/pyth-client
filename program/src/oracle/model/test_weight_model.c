#include <stdio.h>
#include <math.h>
#include "../util/prng.h"

#define WEIGHT_MODEL_NEED_REF
#include "weight_model.h"

int
main( int     argc,
      char ** argv ) {
  (void)argc; (void)argv;

  prng_t _prng[1];
  prng_t * prng = prng_join( prng_new( _prng, (uint32_t)0, (uint64_t)0 ) );

  static long double const _2_30  =     (long double)(UINT64_C(1)<<30);
  static long double const _2_n30 = 1.L/(long double)(UINT64_C(1)<<30);

  uint64_t max_ulp = UINT64_C(0);

  int ctr = 0;
  for( int iter=0; iter<100000000; iter++ ) {
    if( !ctr ) { printf( "Completed %u iterations\n", iter ); ctr = 10000000; }
    ctr--;

    uint32_t t = prng_uint32( prng );
    uint64_t ratio_gap     = prng_uint64( prng ) >> (t & (uint32_t)63); t >>= 6;
    uint64_t ratio_overlap = prng_uint64( prng ) >> (t & (uint32_t)63); t >>= 6;

    uint64_t y = weight_model( ratio_gap, ratio_overlap );
    uint64_t z = (uint64_t)roundl( _2_30*weight_model_ref( _2_n30*(long double)ratio_gap, _2_n30*(long double)ratio_overlap ) );

    uint64_t ulp = y>z ? y-z : z-y;
    if( y>(UINT64_C(1)<<30) || ulp>UINT64_C(1) ) {
      printf( "FAIL (iter %i: gap %lx overlap %lx y %lx z %lx ulp %lu)\n", iter, ratio_gap, ratio_overlap, y, z, ulp );
      return 1;
    }
    if( ulp>max_ulp ) max_ulp = ulp;
  }

  prng_delete( prng_leave( prng ) );

  printf( "pass (max_rerr %.1e max ulp %lu)\n", (double)max_ulp / (double)(1UL<<30), max_ulp );

  return 0;
}

