#include <stdio.h>
#include "util.h"

/* This unit test assumes a uint128_t extensions are available on the
   test platform */

__extension__ typedef          __int128  int128_t;
__extension__ typedef unsigned __int128 uint128_t;

static inline uint64_t split_hi( uint128_t x ) { return (uint64_t)(x>>64); }
static inline uint64_t split_lo( uint128_t x ) { return (uint64_t) x;      }

static inline uint128_t join( uint64_t xh, uint64_t xl ) { return (((uint128_t)xh)<<64) | ((uint128_t)xl); }

static inline uint128_t
prng_uint128( prng_t * prng ) {
  uint64_t h = prng_uint64( prng );
  uint64_t l = prng_uint64( prng );
  return join( h, l );
}

int
main( int     argc,
      char ** argv ) {
  (void)argc; (void)argv;

  prng_t _prng[1];
  prng_t * prng = prng_join( prng_new( _prng, (uint32_t)0, (uint64_t)0 ) );

  int ctr = 0;
  for( int i=0; i<500000000; i++ ) {
    if( !ctr ) { printf( "Completed %i iterations\n", i ); ctr = 10000000; }
    ctr--;

    /* Generate a random test vector */

    uint32_t t = prng_uint32( prng );
    int nx =  1+(((int)t) & 127); t >>= 7; /* Bit width of the x, in [1,128] */
    int ny =  1+(((int)t) & 127); t >>= 7; /* Bit width of the y, in [1,128] */
    int nc =  1+(((int)t) &  63); t >>= 6; /* Bit width of the c, in [1,64] */
    int b0 =     ((int)t) &   1;  t >>= 1; /* Random bit, in [0,1] */
    int b1 =     ((int)t) &   1;  t >>= 1;
    int s  = b1+(((int)t) & 127); t >>= 8; /* Shift magnitude, in [0,128] (0 and 128 at half prob) */

    uint128_t x = prng_uint128( prng ) >> (128-nx); uint64_t xh = split_hi( x ); uint64_t xl = split_lo( x );
    uint128_t y = prng_uint128( prng ) >> (128-ny); uint64_t yh = split_hi( y ); uint64_t yl = split_lo( y );
    uint64_t  c = prng_uint64 ( prng ) >> ( 64-nc);

    /* Add two random uint128_t x and y with a random 64-bit carry in c */

    do {
      uint128_t z0 = (uint128_t)c + x; uint64_t w0 = (uint64_t)(z0<x); z0 += y; w0 += (uint64_t)(z0<y);
      uint64_t zh,zl, w = uwide_add( &zh,&zl, xh,xl, yh,yl, c );
      uint128_t z = join( zh,zl );
      if( z!=z0 || w!=w0 ) {
        printf( "FAIL (iter %i op uwide_add x %016lx %016lx y %016lx %016lx c %016lx z %016lx %016lx w %016lx z0 %016lx %016lx w0 %016lx)\n",
                i, xh,xl, yh,yl, c, zh,zl,w, split_hi(z0),split_lo(z0),w0 );
        return 1;
      }
    } while(0);

    /* Subtract two random uint128_t x and y with a random 64-bit borrow c */

    do {
      uint64_t w0 = (uint64_t)(x<(uint128_t)c); uint128_t z0 = x - (uint128_t)c; w0 += (uint64_t)(z0<y); z0 -= y;
      uint64_t zh,zl, w = uwide_sub( &zh,&zl, xh,xl, yh,yl, c );
      uint128_t z = join( zh,zl );
      if( z!=z0 || w!=w0 ) {
        printf( "FAIL (iter %i op uwide_sub x %016lx %016lx y %016lx %016lx c %016lx z %016lx %016lx w %016lx z0 %016lx %016lx w0 %016lx)\n",
                i, xh,xl, yh,yl, c, zh,zl,w, split_hi(z0),split_lo(z0),w0 );
        return 1;
      }
    } while(0);

    /* Multiply two random uint128_t x and y */

    do {
      uint128_t z0 = ((uint128_t)xl)*((uint128_t)yl);
      uint64_t zh,zl; uwide_mul( &zh,&zl, xl,yl );
      uint128_t z = join( zh,zl );
      if( z!=z0 ) {
        printf( "FAIL (iter %i op uwide_mul x %016lx y %016lx z %016lx %016lx z0 %016lx %016lx)\n",
                i, xl, yl, zh,zl, split_hi(z0),split_lo(z0) );
        return 1;
      }
    } while(0);

    /* Divide a random uint128_t x by a random non-zero d */

    do {
      uint64_t  d = c | (UINT64_C(1) << (nc-1)); /* d is a random nc bit denom with leading 1 set */
      uint128_t z0 = x / (uint128_t)d;
      uint64_t zh,zl; uwide_div( &zh,&zl, xh,xl, d );
      uint128_t z = join( zh,zl );
      if( z!=z0 ) {
        printf( "FAIL (iter %i op uwide_div x %016lx %016lx d %016lx z %016lx %016lx z0 %016lx %016lx)\n",
                i, xh,xl, d, zh,zl, split_hi(z0),split_lo(z0) );
        return 1;
      }
    } while(0);

    /* Compute the log2 of a random uint128_t x (sets leading bit of x)  */

    do {
      x |= ((uint128_t)1) << (nx-1); xh = split_hi( x ); xl = split_lo( x ); /* Set the leading bit */
      int n0 = nx-1;
      int n  = uwide_log2( xh, xl );
      if( n!=n0 ) {
        printf( "FAIL (iter %i op uwide_log2 x %016lx %016lx n %i n0 %i\n", i, xh,xl, n, n0 );
        return 1;
      }
    } while(0);

    /* Compute the log2 of a random uint128_t x with default (clobbers leading bit of x) */

    do {
      if( nx==1 ) { /* If 1 bit wide, use a coin test to we sample the default */
        x = (uint128_t)b0; xh = split_hi( x ); xl = split_lo( x );
        int n0 = (x ? 0 : 1234);
        int n  = uwide_log2_def( xh,xl, 1234 );
        if( n!=n0 ) {
          printf( "FAIL (iter %i op uwide_log2_def x %016lx %016lx n %i n0 %i\n", i, xh,xl, n, n0 );
          return 1;
        }
      } else { /* If wider, def should not happen (assumes leading bit of x set) */
        int n  = uwide_log2_def( xh, xl, 1234 );
        int n0 = nx-1;
        if( n!=n0 ) {
          printf( "FAIL (iter %i op uwide_log2_def x %016lx %016lx n %i n0 %i\n", i, xh,xl, n, n0 );
          return 1;
        }
      }
    } while(0);

    /* Left shift a random uint128_t y */

    do {
      uint128_t z0           = s==128 ? (uint128_t)0 : y<<s;
      int       f0           = (s==0) ? 0 : (s==128) ? !!y : !!(y>>(128-s));
      uint64_t  zh,zl; int f = uwide_sl( &zh,&zl, yh,yl, s );
      uint128_t z            = join( zh,zl );
      if( z!=z0 || f!=f0 ) {
        printf( "FAIL (iter %i op uwide_sl y %016lx %016lx s %i z %016lx %016lx f %i z0 %016lx %016lx f0 %i\n",
                i, yh,yl, s, zh,zl, f, split_hi(z0),split_lo(z0), f0 );
        return 1;
      }
    } while(0);

    /* Right shift a random uint128_t y */

    do {
      uint128_t z0           = s==128 ? (uint128_t)0 : y>>s;
      int       f0           = (s==0) ? 0 : (s==128) ? !!y : !!(y<<(128-s));
      uint64_t  zh,zl; int f = uwide_sr( &zh,&zl, yh,yl, s );
      uint128_t z            = join( zh,zl );
      if( z!=z0 || f!=f0 ) {
        printf( "FAIL (iter %i op uwide_sr y %016lx %016lx s %i z %016lx %016lx f %i z0 %016lx %016lx f0 %i\n",
                i, yh,yl, s, zh,zl, f, split_hi(z0),split_lo(z0), f0 );
        return 1;
      }
    } while(0);
  }

  prng_delete( prng_leave( prng ) );

  printf( "pass\n" );

  return 0;
}

