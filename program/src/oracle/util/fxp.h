#ifndef _pyth_oracle_util_fxp_h_
#define _pyth_oracle_util_fxp_h_

/* Large set of primitives for portable fixed point arithmetic with
   correct rounding and/or overflow detection.  Strongly targeted at
   platforms with reasonable performance C-style 64b unsigned integer
   arithmetic under the hood.  Likewise, strongly targets unsigned fixed
   point representations that fit within 64b and have 30 fractional
   bits. */

#include "uwide.h"

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

#ifdef __cplusplus
}
#endif

#endif /* _pyth_oracle_util_fxp_h_ */