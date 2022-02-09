#include <stdio.h>
#include <math.h>
#include "../util/prng.h"

#define LEAKY_INTEGRATOR_NEED_REF
#include "leaky_integrator.h"

int
main( int     argc,
      char ** argv ) {
  (void)argc; (void)argv;

  prng_t _prng[1];
  prng_t * prng = prng_join( prng_new( _prng, (uint32_t)0, (uint64_t)0 ) );

  long double max_rerr = 0.L;

  static long double const _2_30  =     (long double)(UINT64_C(1)<<30);
  static long double const _2_n30 = 1.L/(long double)(UINT64_C(1)<<30);
  static long double const _2_64  = 18446744073709551616.L;
  static long double const _2_128 = 18446744073709551616.L*18446744073709551616.L;

  int ctr = 0;
  for( int iter=0; iter<100000000; iter++ ) {
    if( !ctr ) { printf( "Completed %u iterations\n", iter ); ctr = 10000000; }
    ctr--;

    uint32_t t = prng_uint32( prng );

    uint64_t yh = prng_uint64( prng );
    uint64_t yl = prng_uint64( prng );
    uwide_sr( &yh,&yl, yh,yl, (int)(t & UINT32_C(127)) ); t >>= 7;
    long double y = (_2_64*(long double)yh) + (long double)yl;

    uint64_t w = (uint64_t)(prng_uint32( prng ) >> (t & UINT32_C(31))); t >>= 5;
    if( w>(UINT64_C(1)<<30) ) w = (UINT64_C(1)<<30);

    uint64_t x = prng_uint64( prng ) >> (t & UINT32_C(63)); t >>= 6;
    
    uint64_t zh,zl; uint64_t co = leaky_integrator( &zh,&zl, yh,yl, w, x );
    long double z = ((long double)co)*_2_128 + ((long double)zh)*_2_64 + ((long double)zl);

    long double z_ref = roundl( leaky_integrator_ref( y, ((long double)w)*_2_n30, _2_30*(long double)x ) );

    long double aerr = fabsl( z-z_ref );
    long double rerr = aerr / fmaxl( 1.L, z_ref );

    if( rerr > 2.6e-14 ) {
      printf( "FAIL (iter %i: y %Le w %lx x %lx z %Le z_ref %Le aerr %.1Le rerr %.1Le\n", iter,
              y, w, x, z, z_ref, aerr, rerr );
      return 1;
    }
    max_rerr = fmaxl( rerr, max_rerr );
  }

  prng_delete( prng_leave( prng ) );

  printf( "pass (max_rerr %.1Le)\n", max_rerr );

  return 0;
}

