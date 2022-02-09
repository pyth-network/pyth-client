#include <stdio.h>
#include <math.h>
#include "../util/prng.h"

#define GAP_MODEL_NEED_REF
#define EXP2M1_FXP_ORDER 7 /* EXP2M1_FXP_ORDER 4 yields a better than IEEE single precision accurate approximation */
#include "gap_model.h"

static uint64_t const ulp_thresh[8] = {
  UINT64_C(0),
  UINT64_C(1260649965), /* max_rerr ~ 3.9e-04 */
  UINT64_C(95149836),   /* max_rerr ~ 3.0e-05 */
  UINT64_C(1348998),    /* max_rerr ~ 4.2e-07 */
  UINT64_C(67273),      /* max_rerr ~ 2.1e-08 */
  UINT64_C(3331),       /* max_rerr ~ 1.0e-09 */
  UINT64_C(40),         /* max_rerr ~ 1.2e-11 */
  UINT64_C(21)          /* max_rerr ~ 6.5e-12 */
};

int
main( int     argc,
      char ** argv ) {
  (void)argc; (void)argv;

  prng_t _prng[1];
  prng_t * prng = prng_join( prng_new( _prng, (uint32_t)0, (uint64_t)0 ) );

  uint64_t max_ulp = UINT64_C(0);
  uint64_t thresh  = ulp_thresh[ EXP2M1_FXP_ORDER ];

  for( int i=0; i<100*GAP_MODEL_LAMBDA; i++ ) {

    uint64_t x;
    if( i<(22*GAP_MODEL_LAMBDA) ) x = (uint64_t)i;
    else {
      int s = ((int)prng_uint32( prng )) & 63;
      x = prng_uint64( prng ) >> s;
    }

    uint64_t y = gap_model( x );
    uint64_t z = (uint64_t)roundl( gap_model_ref( x )*(long double)(1<<30) );

    if( !x ) {
      if( y!=z ) { printf( "FAIL (iter %i: special x %lu y %lu z %lu)\n", i, x, y, z ); return 1; }
      continue;
    }
    if( y>(((uint64_t)GAP_MODEL_LAMBDA)<<30) ) { printf( "FAIL (iter %i: range x %lu y %lu z %lu)\n", i, x, y, z ); return 1; }

    uint64_t ulp = y>z ? (y-z) : (z-y);
    if( ulp>thresh ) { printf( "FAIL (iter %i: ulp x %lu y %lu z %lu ulp %lu)\n", i, x, y, z, ulp ); return 1; }
    if( ulp>max_ulp ) max_ulp = ulp;
  }

  prng_delete( prng_leave( prng ) );

  printf( "pass (max_rerr %.1e max ulp %lu)\n", (double)max_ulp / (((double)(1<<30))*((double)GAP_MODEL_LAMBDA)), max_ulp );

  return 0;
}

