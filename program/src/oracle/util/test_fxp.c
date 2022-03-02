#include <stdio.h>
#include <math.h>
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

static inline int
test_fxp_sqrt_rtz( uint64_t x,
                   uint64_t y ) {
  if( !x ) return !!y;
  if( !(((UINT64_C(1)<<15)<=y) && (y<=(UINT64_C(1)<<(32+15)))) ) return 1;
  uint64_t xh,xl; uwide_sl ( &xh,&xl, UINT64_C(0),x, 30 );      /* 2^30 x */
  uint64_t rh,rl; uwide_mul( &rh,&rl, y,y );                    /* y^2 */
  uint64_t b = uwide_sub( &rh,&rl, xh,xl, rh,rl, UINT64_C(0) ); /* r = 2^30 x - y^2, in [0,2y] if rounded correctly */
  return b || rh || rl>(y<<1);
}

static inline int
test_fxp_sqrt_raz( uint64_t x,
                   uint64_t y ) {
  if( !x ) return !!y;
  if( !(((UINT64_C(1)<<15)<=y) && (y<=(UINT64_C(1)<<(32+15)))) ) return 1;
  uint64_t xh,xl; uwide_sl ( &xh,&xl, UINT64_C(0),x, 30 );      /* 2^30 x */
  uint64_t rh,rl; uwide_mul( &rh,&rl, y,y );                    /* y^2 */
  uint64_t b = uwide_sub( &rh,&rl, rh,rl, xh,xl, UINT64_C(0) ); /* y^2 - 2^30 x, in [0,2y-2] if rounded correctly */
  return b || rh || rl>((y<<1)-UINT64_C(2));
}

static inline int
test_fxp_sqrt_rnz( uint64_t x,
                   uint64_t y ) {
  if( !x ) return !!y;
  if( !(((UINT64_C(1)<<15)<=y) && (y<=(UINT64_C(1)<<(32+15)))) ) return 1;
  uint64_t xh,xl; uwide_sl ( &xh,&xl, UINT64_C(0),x, 30 );      /* 2^30 x */
  uint64_t rh,rl; uwide_mul( &rh,&rl, y,y-UINT64_C(1) );        /* y^2 - y */
  uwide_inc( &rh,&rl, rh,rl, UINT64_C(1) );                     /* y^2 - y + 1 */
  uint64_t b = uwide_sub( &rh,&rl, xh,xl, rh,rl, UINT64_C(0) ); /* r = 2^30 x - (y^2 - y + 1), in [0,2y-1] if rounded correctly */
  return b || rh || rl>=(y<<1);
}

static inline int
fxp_log2_ref( uint64_t * _f,
              uint64_t   x ) {
  if( !x ) { *_f = UINT64_C(0); return INT_MIN; }
  long double ef = log2l( (long double)x );
  int e = log2_uint64( x );
  *_f = (uint64_t)roundl( (ef - (long double)e)*(long double)(UINT64_C(1)<<30) );
  return e-30;
}

static inline uint64_t
fxp_exp2_ref( uint64_t x ) {
  if( x>=UINT64_C(0x880000000) ) return UINT64_MAX;
  return (uint64_t)roundl( ((long double)(UINT64_C(1)<<30))*exp2l( ((long double)x)*(1.L/(long double)(UINT64_C(1)<<30)) ) );
}

static inline uint64_t
fxp_rexp2_ref( uint64_t x ) {
  return (uint64_t)roundl( ((long double)(UINT64_C(1)<<30))*exp2l( ((long double)x)*(-1.L/(long double)(UINT64_C(1)<<30)) ) );
}

int
main( int     argc,
      char ** argv ) {
  (void)argc; (void)argv;

  prng_t _prng[1];
  prng_t * prng = prng_join( prng_new( _prng, (uint32_t)0, (uint64_t)0 ) );

  uint64_t fxp_log2_approx_ulp  = UINT64_C(0);
  uint64_t fxp_exp2_approx_ulp  = UINT64_C(0);
  uint64_t fxp_rexp2_approx_ulp = UINT64_C(0);

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
      uint64_t z1 = fxp_##op       ( x ); \
      uint64_t z2 = fxp_##op##_fast( x ); \
      if( test_fxp_##op( x, z1 ) || ((x<UINT64_C(0x400000000)) && (z1!=z2)) ) { \
        printf( "%i: FAIL (fxp_" #op " x %016lx z1 %016lx z2 %016lx\n", \
                i, x, z1, z2 );           \
        return 1;                         \
      }                                   \
    } while(0)

    TEST(sqrt_rtz);
    TEST(sqrt_raz);
    TEST(sqrt_rnz);

#   undef TEST

    do {
      uint64_t f0; int e0 = fxp_log2_ref   ( &f0, x );
      uint64_t f1; int e1 = fxp_log2_approx( &f1, x );
      uint64_t ulp = f0>f1 ? f0-f1 : f1-f0;
      if( ulp > fxp_log2_approx_ulp ) fxp_log2_approx_ulp = ulp;
      if( e0!=e1 || ulp>UINT64_C(2) ) {
        printf( "%i: FAIL (fxp_log2_approx x %016lx z0 %3i %016lx z1 %3i %016lx ulp %016lx\n", i, x, e0,f0, e1,f1, ulp );
        return 1;
      }
    } while(0);

    do {
      uint64_t z0  = fxp_exp2_ref   ( x );
      uint64_t z1  = fxp_exp2_approx( x );
      uint64_t ulp = z0>z1 ? z0-z1 : z1-z0;
      uint64_t ix = x>>30; /* Larger x are only +/-1 ulp accurate in the first 30 bits */
      if( UINT64_C(0)<ix && ix<UINT64_C(64) ) ulp >>= ix;
      if( ulp > fxp_exp2_approx_ulp ) fxp_exp2_approx_ulp = ulp;
      if( ulp>UINT64_C(1) ) {
        printf( "%i: FAIL (fxp_exp2_approx x %016lx z0 %016lx z1 %016lx ulp %016lx\n", i, x, z0, z1, ulp );
        return 1;
      }
    } while(0);

    do {
      uint64_t z0 = fxp_rexp2_ref   ( x );
      uint64_t z1 = fxp_rexp2_approx( x );
      uint64_t ulp = z0>z1 ? z0-z1 : z1-z0;
      if( ulp > fxp_rexp2_approx_ulp ) fxp_rexp2_approx_ulp = ulp;
      if( ulp>UINT64_C(1) ) {
        printf( "%i: FAIL (fxp_rexp2_approx x %016lx z0 %016lx z1 %016lx ulp %016lx\n", i, x, z0, z1, ulp );
        return 1;
      }
    } while(0);

  }

  prng_delete( prng_leave( prng ) );

  printf( "pass (ulp fxp_log2_approx ~ %lu fxp_exp2_approx ~ %lu fxp_rexp2_approx ~ %lu)\n",
          fxp_log2_approx_ulp, fxp_exp2_approx_ulp, fxp_rexp2_approx_ulp );

  return 0;
}

