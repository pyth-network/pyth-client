#include <stdio.h>
#include <math.h>
#include <fenv.h>
#include "prng.h"

#include "fxp.h"

/* Create random bit patterns with lots of leading and/or trailing zeros
   or ones to really stress limits of implementations. */

static inline uint64_t            /* Random fxp */
make_rand_fxp( uint64_t x,        /* Random 64-bit */
               uint32_t *_ctl ) { /* Least significant 8 bits random, uses them up */
  uint32_t ctl = *_ctl;
  int s = (int)(ctl & UINT32_C(63)); ctl >>= 6; /* Shift, in [0,63] */
  int d = (int)(ctl & UINT32_C( 1)); ctl >>= 1; /* Direction, in [0,1] */
  int i = (int)(ctl & UINT32_C( 1)); ctl >>= 1; /* Invert, in [0,1] */
  *_ctl = ctl;
  x = d ? (x<<s) : (x>>s);
  return i ? (~x) : x;
}

__extension__ typedef unsigned __int128 uint128_t;

static inline uint64_t split_hi( uint128_t x ) { return (uint64_t)(x>>64); }
static inline uint64_t split_lo( uint128_t x ) { return (uint64_t) x;      }

static inline uint64_t
fxp_add_ref( uint64_t   x,
             uint64_t   y,
             uint64_t * _c ) {
  uint128_t z = ((uint128_t)x) + ((uint128_t)y);
  *_c = split_hi( z );
  return split_lo( z );
}

static inline uint64_t
fxp_sub_ref( uint64_t   x,
             uint64_t   y,
             uint64_t * _b ) {
  uint64_t  b = (uint64_t)(x<y);
  uint128_t z = (((uint128_t)b)<<64) + ((uint128_t)x) - ((uint128_t)y);
  *_b = b;
  return split_lo( z );
}

static inline uint64_t
fxp_mul_rtz_ref( uint64_t   x,
                 uint64_t   y,
                 uint64_t * _c ) {
  uint128_t z = ((uint128_t)x)*((uint128_t)y);
  z >>= 30;
  *_c = split_hi( z );
  return split_lo( z );
}

static inline uint64_t
fxp_mul_raz_ref( uint64_t   x,
                 uint64_t   y,
                 uint64_t * _c ) {
  uint128_t z = (((uint128_t)x)*((uint128_t)y) + ((uint128_t)((UINT64_C(1)<<30)-UINT64_C(1)))) >> 30;
  *_c = split_hi( z );
  return split_lo( z );
}

static inline uint64_t
fxp_mul_rnz_ref( uint64_t   x,
                 uint64_t   y,
                 uint64_t * _c ) {
  uint128_t z = (((uint128_t)x)*((uint128_t)y) + ((uint128_t)((UINT64_C(1)<<29)-UINT64_C(1)))) >> 30;
  *_c = split_hi( z );
  return split_lo( z );
}

static inline uint64_t
fxp_mul_rna_ref( uint64_t   x,
                 uint64_t   y,
                 uint64_t * _c ) {
  uint128_t z = (((uint128_t)x)*((uint128_t)y) + ((uint128_t)(UINT64_C(1)<<29))) >> 30;
  *_c = split_hi( z );
  return split_lo( z );
}

static inline uint64_t
fxp_mul_rne_ref( uint64_t   x,
                 uint64_t   y,
                 uint64_t * _c ) {
  uint128_t z = ((uint128_t)x)*((uint128_t)y);
  uint64_t  f = split_lo( z ) & ((UINT64_C(1)<<30)-UINT64_C(1));
  z >>= 30;
  if( (f>(UINT64_C(1)<<29)) || ((f==(UINT64_C(1)<<29)) && (z & UINT64_C(1))) ) z++;
  *_c = split_hi( z );
  return split_lo( z );
}

static inline uint64_t
fxp_mul_rno_ref( uint64_t   x,
                 uint64_t   y,
                 uint64_t * _c ) {
  uint128_t z = ((uint128_t)x)*((uint128_t)y);
  uint64_t  f = split_lo( z ) & ((UINT64_C(1)<<30)-UINT64_C(1));
  z >>= 30;
  if( (f>(UINT64_C(1)<<29)) || ((f==(UINT64_C(1)<<29)) && !(z & UINT64_C(1))) ) z++;
  *_c = split_hi( z );
  return split_lo( z );
}

static inline uint64_t
fxp_div_rtz_ref( uint64_t   x,
                 uint64_t   _y,
                 uint64_t * _c ) {
  if( !_y ) { *_c = UINT64_MAX; return UINT64_C(0); }
  uint128_t ex = ((uint128_t)x) << 30;
  uint128_t y  = (uint128_t)_y;
  uint128_t z  = ex / y;
  *_c = split_hi( z );
  return split_lo( z );
}

static inline uint64_t
fxp_div_raz_ref( uint64_t   x,
                 uint64_t   _y,
                 uint64_t * _c ) {
  if( !_y ) { *_c = UINT64_MAX; return UINT64_C(0); }
  uint128_t ex = ((uint128_t)x) << 30;
  uint128_t y  = (uint128_t)_y;
  uint128_t z  = (ex + y - (uint128_t)1) / y;
  *_c = split_hi( z );
  return split_lo( z );
}

static inline uint64_t
fxp_div_rnz_ref( uint64_t   x,
                 uint64_t   _y,
                 uint64_t * _c ) {
  if( !_y ) { *_c = UINT64_MAX; return UINT64_C(0); }
  uint128_t ex = ((uint128_t)x) << 30;
  uint128_t y  = (uint128_t)_y;
  uint128_t z  = (ex + ((y-(uint128_t)1)>>1)) / y;
  *_c = split_hi( z );
  return split_lo( z );
}

static inline uint64_t
fxp_div_rna_ref( uint64_t   x,
                 uint64_t   _y,
                 uint64_t * _c ) {
  if( !_y ) { *_c = UINT64_MAX; return UINT64_C(0); }
  uint128_t ex = ((uint128_t)x) << 30;
  uint128_t y  = (uint128_t)_y;
  uint128_t z  = (ex + (y>>1)) / y;
  *_c = split_hi( z );
  return split_lo( z );
}

static inline uint64_t
fxp_div_rne_ref( uint64_t   x,
                 uint64_t   _y,
                 uint64_t * _c ) {
  if( !_y ) { *_c = UINT64_MAX; return UINT64_C(0); }
  uint128_t ex = ((uint128_t)x) << 30;
  uint128_t y  = (uint128_t)_y;
  uint128_t z  = ex / y;
  uint128_t r2 = (ex - z*y) << 1;
  if( r2>y || (r2==y && (z & (uint128_t)1)) ) z++;
  *_c = split_hi( z );
  return split_lo( z );
}

static inline uint64_t
fxp_div_rno_ref( uint64_t   x,
                 uint64_t   _y,
                 uint64_t * _c ) {
  if( !_y ) { *_c = UINT64_MAX; return UINT64_C(0); }
  uint128_t ex = ((uint128_t)x) << 30;
  uint128_t y  = (uint128_t)_y;
  uint128_t z  = ex / y;
  uint128_t r2 = (ex - z*y) << 1;
  if( r2>y || (r2==y && !(z & (uint128_t)1)) ) z++;
  *_c = split_hi( z );
  return split_lo( z );
}

static inline uint64_t
fxp_sqrt_rtz_ref( uint64_t x ) {
  static long double const c = (long double)(UINT64_C(1)<<30);
  return (uint64_t)floorl( sqrtl( c*(long double)x ) );
}

int
main( int     argc,
      char ** argv ) {
  (void)argc; (void)argv;

  prng_t _prng[1];
  prng_t * prng = prng_join( prng_new( _prng, (uint32_t)0, (uint64_t)0 ) );

  fesetround( FE_TOWARDZERO );

  int ctr = 0;
  for( int i=0; i<100000000; i++ ) {
    if( !ctr ) { printf( "Completed %i iterations\n", i ); ctr = 10000000; }
    ctr--;

    uint32_t t =                prng_uint32( prng );
    uint64_t x = make_rand_fxp( prng_uint64( prng ), &t );
    uint64_t y = make_rand_fxp( prng_uint64( prng ), &t );

#   define TEST(op)                                  \
    do {                                             \
      uint64_t c0,z0 = fxp_##op##_ref ( x, y, &c0 ); \
      uint64_t c1,z1 = fxp_##op       ( x, y, &c1 ); \
      uint64_t    z2 = fxp_##op##_fast( x, y );      \
      if( c0!=c1 || z0!=z1 || (!c0 && z0!=z2) ) {    \
        printf( "%i: FAIL (fxp_" #op " x %016lx y %016lx cz0 %016lx %016lx cz1 %016lx %016lx z2 %016lx\n", \
                i, x, y, c0,z0, c1,z1, z2 );         \
        return 1;                                    \
      }                                              \
    } while(0)

    TEST(add);
    TEST(sub);

#   undef TEST
#   define TEST(op)                                  \
    do {                                             \
      uint64_t c0,z0 = fxp_##op##_ref ( x, y, &c0 ); \
      uint64_t c1,z1 = fxp_##op       ( x, y, &c1 ); \
      uint64_t    z2 = fxp_##op##_fast( x, y );      \
      if( c0!=c1 || z0!=z1 || (!c0 && z0<UINT64_C(0x3c0000000) && z0!=z2) ) { \
        printf( "%i: FAIL (fxp_" #op " x %016lx y %016lx cz0 %016lx %016lx cz1 %016lx %016lx z2 %016lx\n", \
                i, x, y, c0,z0, c1,z1, z2 );         \
        return 1;                                    \
      }                                              \
    } while(0)

    TEST(mul_rtz);
    TEST(mul_raz);
    TEST(mul_rnz);
    TEST(mul_rna);
    TEST(mul_rne);
    TEST(mul_rno);

#   undef TEST
#   define TEST(op)                                  \
    do {                                             \
      uint64_t c0,z0 = fxp_##op##_ref ( x, y, &c0 ); \
      uint64_t c1,z1 = fxp_##op       ( x, y, &c1 ); \
      uint64_t    z2 = y ? fxp_##op##_fast( x, y ) : UINT64_C(0); \
      if( c0!=c1 || z0!=z1 || ((x<UINT64_C(0x400000000)) && (y<=UINT64_MAX-(x<<30)) && (z0!=z2)) ) { \
        printf( "%i: FAIL (fxp_" #op " x %016lx y %016lx cz0 %016lx %016lx cz1 %016lx %016lx z2 %016lx\n", \
                i, x, y, c0,z0, c1,z1, z2 );         \
        return 1;                                    \
      }                                              \
    } while(0)

    TEST(div_rtz);
    TEST(div_raz);
    TEST(div_rnz);
    TEST(div_rna);
    TEST(div_rne);
    TEST(div_rno);

#   undef TEST
#   define TEST(op)                       \
    do {                                  \
      uint64_t z0 = fxp_##op##_ref ( x ); \
      uint64_t z1 = fxp_##op       ( x ); \
      uint64_t z2 = fxp_##op##_fast( x ); \
      if( z0!=z1 || ((x<UINT64_C(0x400000000)) && (z0!=z2)) ) { \
        printf( "%i: FAIL (fxp_" #op " x %016lx z0 %016lx z1 %016lx z2 %016lx\n", \
                i, x, z0, z1, z2 );       \
        return 1;                         \
      }                                   \
    } while(0)

    TEST(sqrt_rtz);

#   undef TEST
  }

  prng_delete( prng_leave( prng ) );

  printf( "pass\n" );

  return 0;
}

