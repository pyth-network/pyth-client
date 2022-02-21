#ifndef _pyth_oracle_util_fxp_h_
#define _pyth_oracle_util_fxp_h_

/* Large set of primitives for portable fixed point arithmetic with
   correct rounding and/or overflow detection.  Strongly targeted at
   platforms with reasonable performance C-style 64b unsigned integer
   arithmetic under the hood.  Likewise, strongly targets unsigned fixed
   point representations that fit within 64b and have 30 fractional
   bits. */

#include <limits.h> /* For INT_MIN */
#include "uwide.h"
#include "sqrt.h"

#ifdef __cplusplus
extern "C" {
#endif

/* FIXED POINT ADDITION ***********************************************/

/* Compute:
     (c 2^64 + z)/2^30 = x/2^30 + y/2^30
   Exact.  c will be in [0,1].  Fast variant assumes that the user knows
   c is zero or is not needed. */

static inline uint64_t fxp_add( uint64_t x, uint64_t y, uint64_t * _c ) { *_c = (uint64_t)(x>(~y)); return x + y; }
static inline uint64_t fxp_add_fast( uint64_t x, uint64_t y ) { return x + y; }

/* FIXED POINT SUBTRACTION ********************************************/

/* Compute:
     z/2^30 = (2^64 b + x)/2^30 - y/2^30
   Exact.  b will be in [0,1].  Fast variant assumes that the user knows
   b is zero (i.e. x>=y) or is not needed. */

static inline uint64_t fxp_sub( uint64_t x, uint64_t y, uint64_t * _b ) { *_b = (uint64_t)(x<y); return x - y; }
static inline uint64_t fxp_sub_fast( uint64_t x, uint64_t y ) { return x - y; }

/* FIXED POINT MULTIPLICATION *****************************************/

/* Compute:
     (2^64 c + z)/2^30 ~ (x/2^30)(y/2^30)
   under various rounding modes.  c<2^34. */

/* Helper used by the below that computes
     2^64 c + y = floor( (2^64 xh + xl) / 2^30 )
   (i.e. shift a 128b uint right by 30).  Exact.  c<2^34. */

static inline uint64_t fxp_mul_contract( uint64_t xh, uint64_t xl, uint64_t * _c ) { *_c = xh>>30; return (xh<<34) | (xl>>30); }

/* rtz -> Round toward zero (aka truncate rounding)
   Fast variant assumes x*y < 2^64 (i.e. exact result < ~2^4).
   Based on:
        z/2^30 ~ (x/2^30)(y/2^30)
     -> z      ~ x y / 2^30
   With RTZ rounding: 
        z      = floor( x y / 2^30 )
               = (x*y) >> 30
   As x*y might overflow 64 bits, we need to do a 64b*64b->128b
   multiplication (uwide_mul) and a 128b shift right by 30
   (fxp_mul_contract).
   
   Fastest style of rounding.  Rounding error in [0,1) ulp.
   (ulp==2^-30). */

static inline uint64_t
fxp_mul_rtz( uint64_t   x,
             uint64_t   y,
             uint64_t * _c ) {
  uint64_t zh,zl; uwide_mul( &zh,&zl, x, y ); /* <= 2^128 - 2^65 + 1 so no overflow */
  return fxp_mul_contract( zh,zl, _c );
}

static inline uint64_t fxp_mul_rtz_fast( uint64_t x, uint64_t y ) { return (x*y) >> 30; }

/* raz -> Round away from zero
   Fast variant assumes x*y < 2^64-2^30+1 (i.e. exact result < ~2^4)
   Based on:
        z/2^30 ~ (x/2^30)(y/2^30)
     -> z      ~ x y / 2^30
   With RAZ rounding: 
        z      = ceil( x y / 2^30 )
               = floor( (x y + 2^30 - 1) / 2^30 )
               = (x*y + 2^30 -1) >> 30
   As x*y might overflow 64 bits, we need to do a 64b*64b->128b
   multiplication (uwide_mul), a 64b increment of a 128b (uwide_inc) and
   a 128b shift right by 30 (fxp_mul_contract).
   
   Slightly more expensive (one uwide_inc) than RTZ rounding.  Rounding
   error in (-1,0] ulp.  (ulp==2^-30). */

static inline uint64_t
fxp_mul_raz( uint64_t   x,
             uint64_t   y,
             uint64_t * _c ) {
  uint64_t zh,zl; uwide_mul( &zh,&zl, x, y );                 /* <= 2^128 - 2^65 + 1 so no overflow */
  uwide_inc( &zh,&zl, zh,zl, (UINT64_C(1)<<30)-UINT64_C(1) ); /* <= 2^128 - 2^65 + 2^30 so no overflow */
  return fxp_mul_contract( zh,zl, _c );
}

static inline uint64_t fxp_mul_raz_fast( uint64_t x, uint64_t y ) { return (x*y+((UINT64_C(1)<<30)-UINT64_C(1))) >> 30; }

/* rnz -> Round nearest with ties toward zero
   Fast variant assumes x*y < 2^64-2^29+1 (i.e. exact result < ~2^4)
   Based on:
        z/2^30 ~ (x/2^30)(y/2^30)
     -> z      ~ x y / 2^30
   Let frac be the least significant 30 bits of x*y.  If frac<2^29
   (frac>2^29), we should round down (up).  frac==2^29 is a tie and we
   should round down.  If we add 2^29-1 to frac, the result will be
   <2^30 when frac<=2^29 and will be >=2^30.  Thus bit 30 of frac+2^29-1
   indicates whether we need to round up or down.  This yields:
        z = floor( x y + 2^29 - 1 ) / 2^30 )
          = (x*y + 2^29 - 1) >> 30
   As x*y might overflow 64 bits, we need to do a 64b*64b->128b
   multiplication (uwide_mul), a 64b increment of a 128b (uwide_inc) and
   a 128b shift right by 30 (fxp_mul_contract).

   Slightly more expensive (one uwide_inc) than RTZ rounding.  Rounding
   error in (-1/2,1/2] ulp.  (ulp==2^-30). */

static inline uint64_t
fxp_mul_rnz( uint64_t   x,
             uint64_t   y,
             uint64_t * _c ) {
  uint64_t zh,zl; uwide_mul( &zh,&zl, x, y );                 /* <= 2^128 - 2^65 + 1 so no overflow */
  uwide_inc( &zh,&zl, zh,zl, (UINT64_C(1)<<29)-UINT64_C(1) ); /* <= 2^128 - 2^65 + 2^29 so no overflow */
  return fxp_mul_contract( zh,zl, _c );
}

static inline uint64_t fxp_mul_rnz_fast( uint64_t x, uint64_t y ) { return (x*y+((UINT64_C(1)<<29)-UINT64_C(1))) >> 30; }

/* rna -> Round nearest with ties away from zero (aka grade school rounding)
   Fast variant assumes x*y < 2^64-2^29 (i.e. exact result < ~2^4)
   Based on:
        z/2^30 ~ (x/2^30)(y/2^30)
     -> z      ~ x y / 2^30
   Let frac be the least significant 30 bits of x*y.  If frac<2^29
   (frac>2^29), we should round down (up).  frac==2^29 is a tie and we
   should round up.  If we add 2^29-1 to frac, the result will be <2^30
   when frac<=2^29 and will be >=2^30.  Thus bit 30 of frac+2^29
   indicates whether we need to round up or down.  This yields:
        z = floor( x y + 2^29 ) / 2^30 )
          = (x*y + 2^29) >> 30
   As x*y might overflow 64 bits, we need to do a 64b*64b->128b
   multiplication (uwide_mul), a 64b increment of a 128b (uwide_inc) and
   a 128b shift right by 30 (fxp_mul_contract).

   Slightly more expensive (one uwide_inc) than RTZ rounding.  Rounding
   error in [-1/2,1/2) ulp.  (ulp==2^-30). */

static inline uint64_t
fxp_mul_rna( uint64_t   x,
             uint64_t   y,
             uint64_t * _c ) {
  uint64_t zh,zl; uwide_mul( &zh,&zl, x, y );   /* <= 2^128 - 2^65 + 1 so no overflow */
  uwide_inc( &zh,&zl, zh,zl, UINT64_C(1)<<29 ); /* <= 2^128 - 2^65 + 2^29 so no overflow */
  return fxp_mul_contract( zh,zl, _c );
}

static inline uint64_t fxp_mul_rna_fast( uint64_t x, uint64_t y ) { return (x*y+(UINT64_C(1)<<29)) >> 30; }

/* rne -> Round toward nearest with ties toward even (aka banker's rounding / IEEE style rounding)
   Fast variant assumes x*y < 2^64-2^29 (i.e. exact result < ~2^4)
   Based on the observation that rnz / rna rounding should be used when
   floor(x*y/2^30) is even/odd.  That is, use the rnz / rna increment of
   2^29-1 / 2^29 when bit 30 of x*y is 0 / 1.  As x*y might overflow 64
   bits, we need to do a 64b*64b->128b multiplication (uwide_mul), a 64b
   increment of a 128b (uwide_inc) and a 128b shift right by 30
   (fxp_mul_contract).
   
   The most accurate style of rounding usually and somewhat more
   expensive (some sequentially dependent bit ops and one uwide_inc)
   than RTZ rounding.  Rounding error in [-1/2,1/2] ulp (unbiased).
   (ulp==2^-30). */

static inline uint64_t
fxp_mul_rne( uint64_t   x,
             uint64_t   y,
             uint64_t * _c ) {
  uint64_t zh,zl; uwide_mul( &zh,&zl, x, y );                              /* <= 2^128 - 2^65 + 1 */
  uint64_t t = ((UINT64_C(1)<<29)-UINT64_C(1)) + ((zl>>30) & UINT64_C(1)); /* t = 2^29-1 / 2^29 when bit 30 of x*y is 0 / 1 */
  uwide_inc( &zh,&zl, zh,zl, t );                                          /* <= 2^128 - 2^65 + 2^29 */
  return fxp_mul_contract( zh,zl, _c );
}

static inline uint64_t
fxp_mul_rne_fast( uint64_t x,
                  uint64_t y ) {
  uint64_t z = x*y;
  uint64_t t = ((UINT64_C(1)<<29)-UINT64_C(1)) + ((z>>30) & UINT64_C(1)); /* t = 2^29-1 / 2^29 when bit 30 of x*y is 0 / 1 */
  return (z + t) >> 30;
}

/* rno -> Round toward nearest with ties toward odd
   Fast variant assumes x*y < 2^64-2^29 (i.e. exact result < ~2^4)
   Same as rne with the parity flipped for the increment.  As x*y might
   overflow 64 bits, we need to do a 64b*64b->128b multiplication
   (uwide_mul), a 64b increment of a 128b (uwide_inc) and a 128b shift
   right by 30 (fxp_mul_contract).
   
   Somewhat more expensive (some sequentially dependent bit ops and one
   uwide_inc) than RTZ rounding.  Rounding error in [-1/2,1/2] ulp
   (unbiased).  (ulp==2^-30). */

static inline uint64_t
fxp_mul_rno( uint64_t   x,
             uint64_t   y,
             uint64_t * _c ) {
  uint64_t zh,zl; uwide_mul( &zh,&zl, x, y );                /* <= 2^128 - 2^65 + 1 */
  uint64_t t = (UINT64_C(1)<<29) - ((zl>>30) & UINT64_C(1)); /* t = 2^29 / 2^29-1 when bit 30 of x*y is 0 / 1 */
  uwide_inc( &zh,&zl, zh,zl, t );                            /* <= 2^128 - 2^65 + 2^29 */
  return fxp_mul_contract( zh,zl, _c );
}

static inline uint64_t
fxp_mul_rno_fast( uint64_t x,
                  uint64_t y ) {
  uint64_t z = x*y;
  uint64_t t = (UINT64_C(1)<<29) - ((z>>30) & UINT64_C(1)); /* t = 2^29-1 / 2^29 when bit 30 of x*y is 0 / 1 */
  return (z + t) >> 30;
}

/* Other rounding modes:
     rdn -> Round down                   / toward floor / toward -inf ... same as rtz for unsigned
     rup -> Round up                     / toward ceil  / toward +inf ... same as raz for unsigned
     rnd -> Round nearest with ties down / toward floor / toward -inf ... same as rnz for unsigned
     rnu -> Round nearest with ties up   / toward ceil  / toward -inf ... same as rna for unsigned */

static inline uint64_t fxp_mul_rdn( uint64_t x, uint64_t y, uint64_t * _c ) { return fxp_mul_rtz( x, y, _c ); }
static inline uint64_t fxp_mul_rup( uint64_t x, uint64_t y, uint64_t * _c ) { return fxp_mul_raz( x, y, _c ); }
static inline uint64_t fxp_mul_rnd( uint64_t x, uint64_t y, uint64_t * _c ) { return fxp_mul_rnz( x, y, _c ); }
static inline uint64_t fxp_mul_rnu( uint64_t x, uint64_t y, uint64_t * _c ) { return fxp_mul_rna( x, y, _c ); }

static inline uint64_t fxp_mul_rdn_fast( uint64_t x, uint64_t y ) { return fxp_mul_rtz_fast( x, y ); }
static inline uint64_t fxp_mul_rup_fast( uint64_t x, uint64_t y ) { return fxp_mul_raz_fast( x, y ); }
static inline uint64_t fxp_mul_rnd_fast( uint64_t x, uint64_t y ) { return fxp_mul_rnz_fast( x, y ); }
static inline uint64_t fxp_mul_rnu_fast( uint64_t x, uint64_t y ) { return fxp_mul_rna_fast( x, y ); }

/* FIXED POINT DIVISION ***********************************************/

/* Compute:
     (2^64 c + z)/2^30 ~ (x/2^30)/(y/2^30)
   under various rounding modes.  c<2^30 if y is non-zero.  Returns
   c=UINT64_MAX,z=0 if y is zero. */

/* Helper used by the below that computes
     2^64 yh + yl = 2^30 x
   (i.e. widen a 64b number to 128b and shift it left by 30.)  Exact.
   yh<2^30. */

static inline void fxp_div_expand( uint64_t * _yh, uint64_t * _yl, uint64_t x ) { *_yh = x>>34; *_yl = x<<30; }

/* rtz -> Round toward zero (aka truncate rounding)
   Fast variant assumes y!=0 and x<2^34 (i.e. exact result < ~2^34)
   Based on:
        z/2^30 ~ (x/2^30) / (y/2^30)
     -> z      ~ 2^30 x / y
   With RTZ rounding: 
        z      = floor( 2^30 x / y )
   As 2^30 x might overflow 64 bits, we need to expand x
   (fxp_div_expand) and then use a 128b/64b -> 128b divider.
   (uwide_div).

   Fastest style of rounding.  Rounding error in [0,1) ulp.
   (ulp==2^-30). */

static inline uint64_t
fxp_div_rtz( uint64_t   x,
             uint64_t   y,
             uint64_t * _c ) {
  if( !y ) { *_c = UINT64_MAX; return UINT64_C(0); } /* Handle divide by zero */
  uint64_t zh,zl; fxp_div_expand( &zh,&zl, x );      /* 2^30 x  <= 2^94-2^30 so no overflow */
  uwide_div( &zh,&zl, zh,zl, y );                    /* <zh,zl> <= 2^94-2^30 so no overflow */
  *_c = zh; return zl;
}

static inline uint64_t fxp_div_rtz_fast( uint64_t x, uint64_t y ) { return (x<<30) / y; }

/* raz -> Round away from zero
   Fast variant assumes y!=0 and 2^30*x+y-1<2^64 (i.e. exact result < ~2^34)
   Based on:
        z/2^30 ~ (x/2^30) / (y/2^30)
     -> z      ~ 2^30 x / y
   With RAZ rounding: 
        z      = ceil( 2^30 x / y )
               = floor( (2^30 x + y - 1) / y )
   As 2^30 x might overflow 64 bits, we need to expand x
   (fxp_div_expand), increment it by the 64b y-1 (uwide_inc) and then
   use a 128b/64b->128b divider (uwide_div).

   Slightly more expensive (one uwide_inc) than RTZ rounding.  Rounding
   error in (-1,0] ulp. (ulp==2^-30). */

static inline uint64_t
fxp_div_raz( uint64_t   x,
             uint64_t   y,
             uint64_t * _c ) {
  if( !y ) { *_c = UINT64_MAX; return UINT64_C(0); } /* Handle divide by zero */
  uint64_t zh,zl; fxp_div_expand( &zh,&zl, x );      /* 2^30 x  <= 2^94-2^30 so no overflow */
  uwide_inc( &zh,&zl, zh,zl, y-UINT64_C(1) );        /* <zh,zl> <= 2^94+2^64-2^30-2 so no overflow */
  uwide_div( &zh,&zl, zh,zl, y );                    /* <zh,zl> = ceil( 2^30 x / y ) <= 2^94-2^30 so no overflow */
  *_c = zh; return zl;
}

static inline uint64_t fxp_div_raz_fast( uint64_t x, uint64_t y ) { return ((x<<30)+(y-UINT64_C(1))) / y; }

/* rnz -> Round nearest with ties toward zero
   Fast variant assumes y!=0 and 2^30*x+floor((y-1)/2)<2^64 (i.e. exact result < ~2^34)

   Consider:
     z = floor( (2^30 x + floor( (y-1)/2 )) / y )
   where y>0.
   
   If y is even:                                   odd
     z       = floor( (2^30 x + (y/2) - 1) / y )     = floor( (2^30 x + (y-1)/2) / y )
   or:
     z y + r = 2^30 x + (y/2)-1                      = 2^30 x + (y-1)/2
   for some r in [0,y-1].  Or:
     z y     = 2^30 x + delta                        = 2^30 x + delta                        
   where:
     delta   in [-y/2,y/2-1]                         in [-y/2+1/2,y/2-1/2]
   or:
     z       = 2^30 x / y + epsilon                  = 2^30 x / y + epsilon
   where:
     epsilon in [-1/2,1/2-1/y]                       in [-1/2+1/(2y),1/2-1/(2y)]
   Thus we have:
     2^30 x/y - 1/2 <= z < 2^30 x/y + 1/2            2^30 x/y - 1/2 < z < 2^30 x/y + 1/2

   Combining yields:

     2^30 x/y - 1/2 <= z < 2^30 x/y + 1/2

   Thus, the z computed as per the above is the RNZ rounded result.  As
   2^30 x might overflow 64 bits, we need to expand x (fxp_div_expand),
   increment it by the 64b (y-1)>>1 (uwide_inc) and then use a
   128b/64b->128b divider (uwide_div).

   Slightly more expensive (one uwide_inc) than RTZ rounding.  Rounding
   error in (-1/2,1/2] ulp. (ulp==2^-30). */

static inline uint64_t
fxp_div_rnz( uint64_t   x,
             uint64_t   y,
             uint64_t * _c ) {
  if( !y ) { *_c = UINT64_MAX; return UINT64_C(0); } /* Handle divide by zero */
  uint64_t zh,zl; fxp_div_expand( &zh,&zl, x );      /* 2^30 x <= 2^94-2^30 so no overflow */
  uwide_inc( &zh,&zl, zh,zl, (y-UINT64_C(1))>>1 );   /* <zh,zl> <= 2^94-2^30 + 2^63-1 so no overflow */
  uwide_div( &zh,&zl, zh,zl, y );                    /* <zh,zl> <= ceil(2^30 x/y) <= 2^94-2^30 so no overflow */
  *_c = zh; return zl;
}

static inline uint64_t fxp_div_rnz_fast( uint64_t x, uint64_t y ) { return ((x<<30)+((y-UINT64_C(1))>>1)) / y; }

/* rna -> Round nearest with ties away from zero (aka grade school rounding)
   Fast variant assumes y!=0 and 2^30*x+floor(y/2)<2^64 (i.e. exact result < ~2^34)

   Consider:
     z = floor( (2^30 x + floor( y/2 )) / y )
   where y>0.
   
   If y is even:                                   odd
     z       = floor( (2^30 x + (y/2)) / y )         = floor( (2^30 x + (y-1)/2) / y )
   or:
     z y + r = 2^30 x + (y/2)                        = 2^30 x + (y-1)/2
   for some r in [0,y-1].  Or:
     z y     = 2^30 x + delta                        = 2^30 x + delta                        
   where:
     delta   in [-y/2+1,y/2]                         in [-y/2+1/2,y/2-1/2]
   or:
     z       = 2^30 x / y + epsilon                  = 2^30 x / y + epsilon
   where:
     epsilon in [-1/2+1/y,1/2]                       in [-1/2+1/(2y),1/2-1/(2y)]
   Thus we have:
     2^30 x/y - 1/2 < z <= 2^30 x/y + 1/2            2^30 x/y - 1/2 < z < 2^30 x/y + 1/2

   Combining yields:

     2^30 x/y - 1/2 < z <= 2^30 x/y + 1/2

   Thus, the z computed as per the above is the RNA rounded result.  As
   2^30 x might overflow 64 bits, we need to expand x (fxp_div_expand),
   increment it by the 64b y>>1 (uwide_inc) and then use a
   128b/64b->128b divider (uwide_div).

   Slightly more expensive (one uwide_inc) than RTZ rounding.  Rounding
   error in [-1/2,1/2) ulp. (ulp==2^-30).
   
   Probably worth noting that if y has its least significant bit set,
   all the rnz/rna/rne/rno modes yield the same result (as ties aren't
   possible) and this is the cheapest of the round nearest modes.*/

static inline uint64_t
fxp_div_rna( uint64_t   x,
             uint64_t   y,
             uint64_t * _c ) {
  if( !y ) { *_c = UINT64_MAX; return UINT64_C(0); } /* Handle divide by zero */
  uint64_t zh,zl; fxp_div_expand( &zh,&zl, x );      /* 2^30 x <= 2^94-2^30 so no overflow */
  uwide_inc( &zh,&zl, zh,zl, y>>1 );                 /* <zh,zl> <= 2^94-2^30 + 2^63-1 so no overflow */
  uwide_div( &zh,&zl, zh,zl, y );                    /* <zh,zl> <= ceil(2^30 x/y) <= 2^94-2^30 so no overflow */
  *_c = zh; return zl;
}

static inline uint64_t fxp_div_rna_fast( uint64_t x, uint64_t y ) { return ((x<<30)+(y>>1)) / y; }

/* rne -> Round nearest with ties toward even (aka banker's rounding / IEEE style rounding)
   Fast variant assumes y!=0 and 2^30 x < 2^64 (i.e. exact result < ~2^34)

   Based on computing (when y>0):

     q y + r = 2^30 x

   where q = floor( 2^30 x / y ) and r is in [0,y-1].

   If r < y/2, the result should round down.  And if r > y/2 the result
   should round up.  If r==y/2 (which is only possible if y is even),
   the result should round down / up when q is even / odd.
   
   Combining yields we need to round up when:

     r>floor(y/2) or (r==floor(y/2) and y is even and q is odd)

   As 2^30 x might overflow 64 bits, we need to expand x
   (fxp_div_expand).  Since we need both the 128b quotient and the 64b
   remainder, we need a 128b/64b->128b,64b divider (uwide_divrem ... if
   there was a way to quickly determine if floor( 2^30 x / y ) is even
   or odd, we wouldn't need the remainder and could select the
   appropriate RNZ/RNA based uwide_inc increment) and then a 128b
   conditional increment (uwide_inc).

   The most accurate style of rounding usually and somewhat more
   expensive (needs remainder, some sequentially dependent bit ops and
   one uwide_inc) than RTZ rounding.  Rounding error in [-1/2,1/2] ulp
   (unbiased).  (ulp==2^-30). */

static inline uint64_t
fxp_div_rne( uint64_t   x,
             uint64_t   y,
             uint64_t * _c ) {
  if( !y ) { *_c = UINT64_MAX; return UINT64_C(0); } /* Handle divide by zero */
  uint64_t zh,zl; fxp_div_expand( &zh,&zl, x );      /* 2^30 x <= 2^94-2^30 so no overflow */
  uint64_t r    = uwide_divrem( &zh,&zl, zh,zl, y ); /* <zh,zl>*y + r = 2^30 x where r is in [0,y-1] so no overflow */
  uint64_t flhy = y>>1;                              /* floor(y/2) so no overflow */
  uwide_inc( &zh,&zl, zh,zl, (uint64_t)( (r>flhy) | ((r==flhy) & !!((~y) & zl & UINT64_C(1))) ) ); /* <zh,zl> <= ceil( 2^30 x / y ) <= 2^94-2^30 so no overflow */
  *_c = zh; return zl;
}

static inline uint64_t
fxp_div_rne_fast( uint64_t x,
                  uint64_t y ) {
  uint64_t n    = x << 30;
  uint64_t q    = n / y;
  uint64_t r    = n - q*y;
  uint64_t flhy = y>>1;
  return q + (uint64_t)( (r>flhy) | ((r==flhy) & !!((~y) & q & UINT64_C(1))) );
}

/* rno -> Round nearest with ties toward odd
   Fast variant assumes y!=0 and 2^30 x < 2^64 (i.e. exact result < ~2^34)

   Similar considerations as RNE with the parity for rounding on ties
   swapped.

   Somewhat more expensive (needs remainder, some sequentially dependent
   bit ops and one uwide_inc) than RTZ rounding.  Rounding error in
   [-1/2,1/2] ulp (unbiased).  (ulp==2^-30). */

static inline uint64_t
fxp_div_rno( uint64_t   x,
             uint64_t   y,
             uint64_t * _c ) {
  if( !y ) { *_c = UINT64_MAX; return UINT64_C(0); } /* Handle divide by zero */
  uint64_t zh,zl; fxp_div_expand( &zh,&zl, x );      /* 2^30 x <= 2^94-2^30 so no overflow */
  uint64_t r    = uwide_divrem( &zh,&zl, zh,zl, y ); /* <zh,zl>*y + r = 2^30 x where r is in [0,y-1] so no overflow */
  uint64_t flhy = y>>1;                              /* floor(y/2) so no overflow */
  uwide_inc( &zh,&zl, zh,zl, (uint64_t)( (r>flhy) | ((r==flhy) & !!((~y) & (~zl) & UINT64_C(1))) ) ); /* <zh,zl> <= ceil( 2^30 x / y ) <= 2^94-2^30 so no overflow */
  *_c = zh; return zl;
}

static inline uint64_t
fxp_div_rno_fast( uint64_t x,
                  uint64_t y ) {
  uint64_t n    = x << 30;
  uint64_t q    = n / y;
  uint64_t r    = n - q*y;
  uint64_t flhy = y>>1;
  return q + (uint64_t)( (r>flhy) | ((r==flhy) & !!((~y) & (~q) & UINT64_C(1))) );
}

/* Other rouding modes:
     rdn -> Round down                   / toward floor / toward -inf ... same as rtz for unsigned
     rup -> Round up                     / toward ceil  / toward +inf ... same as raz for unsigned
     rnd -> Round nearest with ties down / toward floor / toward -inf ... same as rnz for unsigned
     rnu -> Round nearest with ties up   / toward ceil  / toward -inf ... same as rna for unsigned */

static inline uint64_t fxp_div_rdn( uint64_t x, uint64_t y, uint64_t * _c ) { return fxp_div_rtz( x, y, _c ); }
static inline uint64_t fxp_div_rup( uint64_t x, uint64_t y, uint64_t * _c ) { return fxp_div_raz( x, y, _c ); }
static inline uint64_t fxp_div_rnd( uint64_t x, uint64_t y, uint64_t * _c ) { return fxp_div_rnz( x, y, _c ); }
static inline uint64_t fxp_div_rnu( uint64_t x, uint64_t y, uint64_t * _c ) { return fxp_div_rna( x, y, _c ); }

static inline uint64_t fxp_div_rdn_fast( uint64_t x, uint64_t y ) { return fxp_div_rtz_fast( x, y ); }
static inline uint64_t fxp_div_rup_fast( uint64_t x, uint64_t y ) { return fxp_div_raz_fast( x, y ); }
static inline uint64_t fxp_div_rnd_fast( uint64_t x, uint64_t y ) { return fxp_div_rnz_fast( x, y ); }
static inline uint64_t fxp_div_rnu_fast( uint64_t x, uint64_t y ) { return fxp_div_rna_fast( x, y ); }

/* FIXED POINT SQRT ***************************************************/

/* Compute:
     z/2^30 ~ sqrt( x/2^30 )
   under various rounding modes. */

/* rtz -> Round toward zero (aka truncate rounding)
   Fast variant assumes x<2^34
   Based on:
        z/2^30 ~ sqrt( x/2^30)
     -> z      ~ sqrt( 2^30 x )
   With RTZ rounding:
        z      = floor( sqrt( 2^30 x ) )
   Fastest style of rounding.  Rounding error in [0,1) ulp.
   (ulp==2^-30). */

static inline uint64_t
fxp_sqrt_rtz( uint64_t x ) {

  /* Initial guess.  Want to compute
       y = sqrt( x 2^30 )
     but x 2^30 does not fit into 64-bits at this point.  So we instead
     approximate:
       y = sqrt( x 2^(2s) 2^(30-2s) )
         = sqrt( x 2^(2s) ) 2^(15-s)
         ~ floor( sqrt( x 2^(2s) ) ) 2^(15-s)
     where s is the largest integer such that x 2^(2s) does not
     overflow. */

  int s = (63-log2_uint64( x )) >> 1;                /* lg x in [34,63], 63-lg x in [0,29], s in [0,14] when x>=2^34 */
  if( s>15 ) s = 15;                                 /* s==15 when x<2^34 */
  uint64_t y = sqrt_uint64( x << (s<<1) ) << (15-s); /* All shifts well defined */
  if( s==15 ) return y;                              /* No iteration if x<2^34 */

  /* Expand x to 2^30 x for the fixed point iteration */
  uint64_t xh,xl; fxp_div_expand( &xh,&xl, x );
  for(;;) {

    /* Iterate y' = floor( (y(y+1) + 2^30 x) / (2y+1) ).  This is the
       same iteration as sqrt_uint{8,16,32,64} (which converges on the
       floor(sqrt(x)) but applied to the (wider than 64b) quantity
       2^30*x and then starting from an exceptionally good guess (such
       that ~2 iterations should be needed at most). */

    uint64_t yh,yl;
    uwide_mul( &yh,&yl, y,y+UINT64_C(1) );
    uwide_add( &yh,&yl, yh,yl, xh,xl, UINT64_C(0) );
    uwide_div( &yh,&yl, yh,yl, (y<<1)+UINT64_C(1) );
    if( yl==y ) break;
    y = yl;
  }

  return y;
}

static inline uint64_t fxp_sqrt_rtz_fast( uint64_t x ) { return sqrt_uint64( x<<30 ); }

/* raz -> Round away zero
   Fast variant assumes x<2^34
   Based on:
        z/2^30 ~ sqrt( x/2^30)
     -> z      ~ sqrt( 2^30 x )
   Let y by the RTZ rounded result:
        y = floor( sqrt( 2^30 x ) )
   and consider the residual:
        r = 2^30 x - y^2
   which, given the above, will be in [0,2y].  If r==0, the result
   is exact and thus already correctly rounded.  Otherwise, we need
   to round up.  We note that the residual of the RTZ iteration is
   the same as this residual at convergence:
        y = floor( (y^2 + y + 2^30 x) / (2y+1) )
          = (y^2 + y + 2^30 x - r') / (2y+1)
   where r' in [0,2y]:
        2y^2 + y = y^2 + y + 2^30 x - r'
        y^2 = 2^30 x - r'
        r' = 2^30 x - y^2
     -> r' = r
   Thus we can use explicitly compute the remainder or use uwide_divrem
   in the iteration itself to produce the needed residual.

   Alternatively, the iteration
        y = floor( (y^2 + y - 2 + 2^30 x) / (2y-1) )
          = floor( (y(y+1) + (2^30 x-2)) / (2y-1) )
   should converge on the RAZ rounded result as:
        y = (y^2 + y - 2 + 2^30 x - r'') / (2y-1)
   where r'' in [0,2y-2]
        2y^2 - y = y^2 + y - 2 + 2^30 x - r''
        y^2 - 2 y + 2 + r'' = 2^30 x
   Thus at r'' = 0:
        (y-1)^2 + 1 = 2^30 x
     -> (y-1)^2 < 2^30 x
   and at r'' = 2y-2
        y^2 = 2^30 x
   such that:
        (y-1)^2 < 2^30 x <= y^2
   which means y is the correctly rounded result.

   Slightly more expensive than RTZ rounding.  Rounding error in (-1,0]
   ulp.  (ulp==2^-30). */

static inline uint64_t
fxp_sqrt_raz( uint64_t x ) {
  uint64_t xh, xl;

  /* Same guess as rtz rounding */
  int s = (63-log2_uint64( x )) >> 1;
  if( s>15 ) s = 15;
  xl = x << (s<<1);
  uint64_t y = sqrt_uint64( xl ) << (15-s);
  if( s==15 ) return y + (uint64_t)!!(xl-y*y); /* Explicitly compute residual to round when no iteration needed */

  /* Use the modified iteration to converge on raz rounded result */
  fxp_div_expand( &xh,&xl, x );
  uwide_dec( &xh,&xl, xh, xl, UINT64_C(2) );
  for(;;) {
    uint64_t yh,yl;
    uwide_mul( &yh,&yl, y, y+UINT64_C(1) );
    uwide_add( &yh,&yl, yh,yl, xh,xl, UINT64_C(0) );
    uwide_div( &yh,&yl, yh,yl, (y<<1)-UINT64_C(1) );
    if( yl==y ) break;
    y = yl;
  }

  return y;
}

static inline uint64_t
fxp_sqrt_raz_fast( uint64_t x ) {
  uint64_t xl = x<<30;
  uint64_t y  = sqrt_uint64( xl );
  uint64_t r  = xl - y*y;
  return y + (uint64_t)!!r;
}

/* rnz/rna/rne/rno -> Round nearest with ties toward zero/away zero/toward even/toward odd
   Fast variant assumes x<2^34
   Based on:
        z/2^30 ~ sqrt( x/2^30)
     -> z      ~ sqrt( 2^30 x )
   Assuming there are no ties, we want to integer z such that:
        (z-1/2)^2 < 2^30 x < (z+1/2)^2
        z^2 - z + 1/4 < 2^30 x < z^2 + z + 1/4
   since z is integral, this is equivalent to finding a z such that:
     -> z^2 - z + 1 <= 2^30 x < z^2 + z + 1
     -> r = 2^30 x - (z^2 - z + 1) and is in [0,2z)
   This suggests using the iteration:
        z = floor( (z^2 + z - 1 + 2^30 x) / (2z) )
          = floor( (z(z+1) + (2^30 x-1)) / (2z) )
   which, at convergence, has:
        2z^2 = z^2 + z - 1 + 2^30 x - r'
   where r' is in [0,2z).  Solving for r', at convergence:
        r' = 2^30 x - (z^2 - z + 1)
        r' = r
   Thus, this iteration converges to the correctly rounded z when there
   are no ties.  But this also demonstrates that no ties are possible
   when z is integral ... the above derivation hold when either endpoint
   of the initial inequality is closed because the endpoint values are
   fraction and cannot be exactly met for any integral z.  As such,
   there are no ties and all round nearest styles can use the same
   iteration for the sqrt function.

   For computing a faster result for small x, let y by the RTZ rounded
   result:
        y = floor( sqrt( 2^30 x ) )
   and consider the residual:
        r'' = 2^30 x - y^2
   which, given the above, will be in [0,2y].  If r''==0, the result is
   exact and thus already correctly rounded.  Otherwise, let:
        z = y when r''<=y and y+1 when when r''>y
   Consider r''' from the above for this z.
        r''' = 2^30 x - z^2
             = 2^30 x - y^2 when r''<=y and 2^30 x - (y+1)^2 o.w.
             = r''' when r''<=y and r'' - 2y - 1 o.w.
     -> r''' in [0,y] when r''<=y and in [y+1,2y]-2y-1 o.w.
     -> r''' in [0,y] when r''<=y and in [-y,-1]-2y-1 o.w.
     -> r''' in [-y,y] and is negative when r''>y
     -> r''' in [-z+1,z]
   This implies that  we have for
        2^30 x - (z^2-z+1) = r''' + z-1 is in [0,2z)
   As such, z computed by this method is also correctly rounded.  Thus
   we can use explicitly compute the remainder or use uwide_divrem in
   the iteration itself to produce the needed residual.

   Very slightly more expensive than RTZ rounding.  Rounding error in
   (-1/2,1/2) ulp.  (ulp==2^-30). */

static inline uint64_t
fxp_sqrt_rnz( uint64_t x ) {
  uint64_t xh, xl;

  /* Same guess as rtz rounding */
  int s = (63-log2_uint64( x )) >> 1;
  if( s>15 ) s = 15;
  xl = x << (s<<1);
  uint64_t y = sqrt_uint64( xl ) << (15-s);
  if( s==15 ) return y + (uint64_t)((xl-y*y)>y); /* Explicitly compute residual to round when no iteration needed */

  /* Use the modified iteration to converge on rnz rounded result */
  fxp_div_expand( &xh,&xl, x );                      /* 2^30 x */
  uwide_dec( &xh,&xl, xh,xl, UINT64_C(1) );          /* 2^30 x - 1 */
  for(;;) {
    uint64_t yh,yl;
    uwide_mul( &yh,&yl, y,y+UINT64_C(1) );           /* y^2 + y */
    uwide_add( &yh,&yl, yh,yl, xh,xl, UINT64_C(0) ); /* y^2 + y - 1 + 2^30 x */
    uwide_div( &yh,&yl, yh,yl, y<<1 );
    if( yl==y ) break;
    y = yl;
  }

  return y;
}

static inline uint64_t
fxp_sqrt_rnz_fast( uint64_t x ) {
  uint64_t xl = x<<30;
  uint64_t y  = sqrt_uint64( xl );
  uint64_t r  = xl - y*y;
  return y + (uint64_t)(r>y);
}

static inline uint64_t fxp_sqrt_rna( uint64_t x ) { return fxp_sqrt_rnz( x ); }
static inline uint64_t fxp_sqrt_rne( uint64_t x ) { return fxp_sqrt_rnz( x ); }
static inline uint64_t fxp_sqrt_rno( uint64_t x ) { return fxp_sqrt_rnz( x ); }

static inline uint64_t fxp_sqrt_rna_fast( uint64_t x ) { return fxp_sqrt_rnz_fast( x ); }
static inline uint64_t fxp_sqrt_rne_fast( uint64_t x ) { return fxp_sqrt_rnz_fast( x ); }
static inline uint64_t fxp_sqrt_rno_fast( uint64_t x ) { return fxp_sqrt_rnz_fast( x ); }

/* Other rouding modes:
     rdn -> Round down                   / toward floor / toward -inf ... same as rtz for unsigned
     rup -> Round up                     / toward ceil  / toward +inf ... same as raz for unsigned
     rnd -> Round nearest with ties down / toward floor / toward -inf ... same as rnz for unsigned
     rnu -> Round nearest with ties up   / toward ceil  / toward -inf ... same as rna for unsigned */

static inline uint64_t fxp_sqrt_rdn( uint64_t x ) { return fxp_sqrt_rtz( x ); }
static inline uint64_t fxp_sqrt_rup( uint64_t x ) { return fxp_sqrt_raz( x ); }
static inline uint64_t fxp_sqrt_rnd( uint64_t x ) { return fxp_sqrt_rnz( x ); }
static inline uint64_t fxp_sqrt_rnu( uint64_t x ) { return fxp_sqrt_rnz( x ); }

static inline uint64_t fxp_sqrt_rdn_fast( uint64_t x ) { return fxp_sqrt_rtz_fast( x ); }
static inline uint64_t fxp_sqrt_rup_fast( uint64_t x ) { return fxp_sqrt_raz_fast( x ); }
static inline uint64_t fxp_sqrt_rnd_fast( uint64_t x ) { return fxp_sqrt_rnz_fast( x ); }
static inline uint64_t fxp_sqrt_rnu_fast( uint64_t x ) { return fxp_sqrt_rnz_fast( x ); }

/* FIXED POINT LOG2 ***************************************************/

/* Compute:

     e + f/2^30 ~ log2( x/2^30 )

   If x is non-zero, e will be in [-30,33] and f will be in [0,2^30]

   Note: This is not guaranteed to be exactly rounded and thus this
   doesn't have variants for every rounding mode under the sun.  The
   current implementation has <=2 ulp error with a round-nearest flavor
   though.

   Given the round-nearest flavor, it is possible to have a f/2^30=1
   exactly (e.g. log2_approx( UINT64_MAX ) will have e=33 and f/2^30=1
   such that e still is exactly determined by the index of x's most
   significant bit but the fractional part is 1 after rounding up.

   It is possible modify this while retaining a round-nearest flavor
   such that e is strictly in [-30,34] and f/2^30 is strictly in [0,1)
   (e will no longer strictly be determined the index of x's most
   significant bit so technically this retains less info than the
   above).

   Likewise, it is possible to modify this to use a round-toward-zero
   flavor such that e will be in [-30,33] and f/2^30 is in [0,1) always.
   The resulting approximation accuracy would be slightly lower and
   slightly biased.

   If x is zero, the result is undefined mathematically (the limit
   x->zero+ is -inf ... this implementation returns i=INT_MIN<<-30 and
   f=0 for specificity. */

static inline int                  /* e, in [-30,33] (==log2_uint64(x)-30 ... see note above) */
fxp_log2_approx( uint64_t * _f,    /* f, in [0,2^30] (yes, closed ... see note above) */
                 uint64_t    x ) {

  /* Handle bad inputs */

  if( !x ) { *_f = UINT64_C(0); return INT_MIN; }

  /* Crack x into:

       x = 2^i ( 1 + y/2^63 )

     where y is in [0,2^63).  This can always be done _exactly_ for
     non-zero x.  That is, i is the index of x's most significant
     non-zero bit (in [0,63]) and y is trailing i bits shifted up to be
     63b wide. */

  int      i = log2_uint64( x );                  /* In [0,63] */
  uint64_t y = (x << (63-i)) - (UINT64_C(1)<<63); /* In [0,2^63) */

  /* Convert this to a fixed point approximation of x:

       x ~ 2^i ( 1 + d/2^30 )

     via:

       d = floor( (y+2^32) / 2^33 )

     This representation is still exact when i <= 30 and at most 1/2 ulp
     error in d when i>30 (rna / round nearest with ties away from zero
     rounding ... consider using ties toward even or truncate rounding
     here as per note above).  Given the use of a round nearest style, d
     is in [0,2^30] (closed at both ends). */

  uint64_t d = (y + (UINT64_C(1)<<32)) >> 33;

  /* Given this, we have:

       e + f/2^30 = log2( x/2^30 )
                  = log2( x ) - 30
                  ~ log2( 2^i ( 1+ d/2^30 ) ) - 30
                  = i-30 + log2( 1 + d/2^30 )

     From this, we identify:

       e      = i-30               (exact)
       f/2^30 ~ log2( 1 + d/2^30 ) (approximate)

     Thus, f of a 30b fixed point lg1p calculator with a 30b fixed point
     input.  The below is automatically generated code for a fixed point
     implementation of a minimax Pade(4,3) approximant to log2(1+x) over
     the domain [0,1].  If exact math, the approximation has an accuracy
     better than 1/2 ulp over the whole domain and is exact at the
     endpoints.  As implemented the accuracy is O(1) ulp over the whole
     domain (with round nearest flavored rounding) style), monotonic and
     still exact at the endpoints. */

  uint64_t f;
  uint64_t g;

  /* BEGIN AUTOGENERATED CODE */
  /* bits 31.8 rms_aerr 1.9e-10 rms_rerr 1.3e-10 max_aerr 2.7e-10 max_rerr 2.7e-10 */

  f = UINT64_C(0x0000000245c36b35);               /* scale 41 bout 34 bmul  - */
  f = UINT64_C(0x000000029c5b8e15) + ((f*d)>>36); /* scale 35 bout 34 bmul 64 */
  f = UINT64_C(0x0000000303d59639) + ((f*d)>>32); /* scale 33 bout 34 bmul 64 */
  f = UINT64_C(0x00000001715475cc) + ((f*d)>>31); /* scale 32 bout 34 bmul 64 */
  f =                                 (f*d);      /* scale 62 bout 64 bmul 64 */
  /* f max 0xd1fb651800000000 */

  g = UINT64_C(0x000000024357c946);               /* scale 37 bout 34 bmul  - */
  g = UINT64_C(0x00000002a94e3723) + ((g*d)>>33); /* scale 34 bout 34 bmul 64 */
  g = UINT64_C(0x000000018b7f484d) + ((g*d)>>32); /* scale 32 bout 34 bmul 64 */
  g = UINT64_C(0x0000000100000000) + ((g*d)>>30); /* scale 32 bout 34 bmul 64 */
  /* g max 0x0000000347ed945f */

  f = (f + (g>>1)) / g; /* RNA style rounding */
  /* END AUTOGENERATED CODE */

  *_f = f; return i-30;
}

#ifdef __cplusplus
}
#endif

#endif /* _pyth_oracle_util_fxp_h_ */
