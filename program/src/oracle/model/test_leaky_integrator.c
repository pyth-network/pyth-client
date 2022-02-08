#include <stdio.h>
#include <math.h>
#include "../util/prng.h"

#include "leaky_integrator.h"

static uint64_t
leaky_integrator_ref( uint64_t * _zh, uint64_t *_zl,
                      uint64_t    yh, uint64_t   yl,
                      uint64_t   _w,
                      uint64_t   _x ) {
  static long double const _2_30  = (long double)(UINT64_C(1)<<30);
  static long double const _2_64  = 18446744073709551616.L;
  static long double const _2_128 = 18446744073709551616.L*18446744073709551616.L;
  static long double const _2_n30 = 1.L/(long double)(UINT64_C(1)<<30);

  /* Using long double because uint64_t not exactly representable in a
     IEEE double.  Due to limited precision of the long doubles, it
     turns out that this calculation is actually less accurate than the
     actual fixed point implementation. */

  long double y = _2_64*((long double)yh) + ((long double)yl);
  long double w = (long double)_w;
  long double x = _2_30*(long double)_x;

  long double z = roundl( (y*w)*_2_n30 + x );

  uint64_t co = (uint64_t)floorl( z*(1.L/_2_128) ); z -= ((long double)co)*_2_128;
  uint64_t zh = (uint64_t)floorl( z*(1.L/_2_64)  ); z -= ((long double)zh)*_2_64;
  uint64_t zl = (uint64_t)floorl( z );

  *_zh = zh;
  *_zl = zl;
  return co;
}

int
main( int     argc,
      char ** argv ) {
  (void)argc; (void)argv;

  prng_t _prng[1];
  prng_t * prng = prng_join( prng_new( _prng, (uint32_t)0, (uint64_t)0 ) );

  long double max_rerr = 0.L;

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

    uint64_t w = (uint64_t)(prng_uint32( prng ) >> (t & UINT32_C(31))); t >>= 5;
    if( w>(UINT64_C(1)<<30) ) w = (UINT64_C(1)<<30);

    uint64_t x = prng_uint64( prng ) >> (t & UINT32_C(63)); t >>= 6;
    
    uint64_t zh,    zl;     uint64_t co     = leaky_integrator    ( &zh,&zl,         yh,yl, w, x );
    uint64_t zh_ref,zl_ref; uint64_t co_ref = leaky_integrator_ref( &zh_ref,&zl_ref, yh,yl, w, x );

    long double z     = ((long double)co    )*_2_128 + ((long double)zh    )*_2_64 + ((long double)zl    );
    long double z_ref = ((long double)co_ref)*_2_128 + ((long double)zh_ref)*_2_64 + ((long double)zl_ref);

    long double rerr  = fabsl( z-z_ref ) / fmaxl( 1.L, z_ref );

    if( rerr > 2.L ) {
      printf( "FAIL (iter %i: y %lx %016lx w %lx x %lx z %lx %016lx %016lx ref %lx %016lx %016lx rerr %.1Le\n", iter,
              yh,yl, w, x, co,zh,zl, co_ref,zh_ref,zl_ref, rerr );
      return 1;
    }
    max_rerr = fmaxl( rerr, max_rerr );
  }

  prng_delete( prng_leave( prng ) );

  printf( "pass (max_rerr %.1Le)\n", max_rerr );

  return 0;
}

