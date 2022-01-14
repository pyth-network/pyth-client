#ifndef _pyth_oracle_util_uwide_h_
#define _pyth_oracle_util_uwide_h_

/* Useful operations for unsigned 128-bit integer operations
   a 128-bit wide number is represented as a pair of two uint64_t.  In
   the notation below <xh,xl> == xh 2^64 + xl where xh and xl are
   uint64_t's. */

#include "log2.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Compute cout 2^128 + <zh,zl> = <xh,xl> + <yh,yl> + c exactly.
   Returns the carry out.  Note that the carry in / carry operations
   should be compile time optimized out if c is 0 at compile time on
   input and/or return value is not used. */

static inline uint64_t
uwide_add( uint64_t * _zh, uint64_t * _zl,
           uint64_t    xh, uint64_t    xl,
           uint64_t    yh, uint64_t    yl,
           uint64_t    c ) {
  uint64_t zl = c + xl; c  = (uint64_t)(zl<xl);
  zl += yl;             c += (uint64_t)(zl<yl);
  uint64_t zh = c + xh; c  = (uint64_t)(zh<xh);
  zh += yh;             c += (uint64_t)(zh<yh);
  *_zh = zh;
  *_zl = zl;
  return c;
}

/* Compute <zh,zl> = x*y exactly.  Return will be in [0,2^128-2^65+1] */

static inline void
uwide_mul( uint64_t * _zh, uint64_t * _zl,
           uint64_t   x,
           uint64_t   y ) {
  uint64_t x1  = x>>32;  uint64_t x0  = (uint64_t)(uint32_t)x;  /* both 2^32-1 @ worst case (x==y==2^64-1) */
  uint64_t y1  = y>>32;  uint64_t y0  = (uint64_t)(uint32_t)y;  /* both 2^32-1 @ worst case */

  uint64_t w0  = x0*y0;  uint64_t w1  = x0*y1;                  /* both 2^64-2^33+1 @ worst case */
  uint64_t w2  = x1*y0;  uint64_t w3  = x1*y1;                  /* both 2^64-2^33+1 @ worst case */

  uint64_t w1h = w1>>32; uint64_t w1l = (uint64_t)(uint32_t)w1; /* w1h 2^32-2, w1l 1 @ worst case */
  uint64_t w2h = w2>>32; uint64_t w2l = (uint64_t)(uint32_t)w2; /* w2h 2^32-2, w2l 1 @ worst case */

  uint64_t zh  = w1h + w2h + w3;                               /* 2^64-3                     @ worst case */
  uint64_t t0  = w0 + (w1l<<32); zh += (uint64_t)(t0<w0);      /* t 2^64-2^32+1, zh 2^64 - 3 @ worst case */
  uint64_t zl  = t0 + (w2l<<32); zh += (uint64_t)(zl<t0);      /* t 1,           zh 2^64 - 2 @ worst case */
  /* zh 2^64 + zl == 2^128-2^65+1 @ worst case */

  *_zh = zh;
  *_zl = zl;
}

/* Compute n = floor( log2 <xh,xl> ) */

static inline int
uwide_log2( uint64_t xh,
            uint64_t xl ) {
  int off = 0;
  if( xh ) { off = 64; xl = xh; }
  return off + log2_uint64( xl );
}

static inline int uwide_log2_def( uint64_t xh, uint64_t xl, int def ) { return (xh|xl) ? uwide_log2(xh,xl) : def; }

/* Compute <zh,zl> = <xh,xl> << s.  Returns the overflow flag (will be 0
   or 1) which indicates if any non-zero bits of xh and xl were lost in
   the shift.  Note that the overflow and various cases should be
   compile time optimized out if if s is known at compile time on input
   and/or return value is not used. */

static inline int
uwide_sl( uint64_t * _zh, uint64_t * _zl,
          uint64_t    xh, uint64_t    xl,
          int s ) { /* U.B. for negative s (currently treats negative s as 0, consider having invalid op flag?) */
  if( s>=128 ) {                        *_zh = UINT64_C(0);     *_zl = UINT64_C(0); return !!(xh| xl    ); }
  if( s>  64 ) { s -= 64; int t = 64-s; *_zh =  xl<<s;          *_zl = UINT64_C(0); return !!(xh|(xl>>t)); }
  if( s== 64 ) {                        *_zh =  xl;             *_zl = UINT64_C(0); return !! xh;          }
  if( s>   0 ) {          int t = 64-s; *_zh = (xh<<s)|(xl>>t); *_zl = xl<<s;       return !!(xh>>t);      }
  /*  s==  0 */                         *_zh =  xh;             *_zl = xl;          return 0;
}

/* Compute <zh,zl> = <xh,xl> >> s.  Returns ane overflow flag (will be 0
   or 1) which indicates if any non-zero bits of xh and xl were lost in
   the shift.  Note that the overflow and various cases should be
   compile time optimized out if if s is known at compile time on input
   and/or return value is not used. */

static inline int
uwide_sr( uint64_t * _zh, uint64_t * _zl,
          uint64_t    xh, uint64_t    xl,
          int s ) { /* U.B. for negative s (currently treats negative s as 0, consider having invalid op flag?) */
  if( s>=128 ) {                        *_zh = UINT64_C(0); *_zl = UINT64_C(0);     return !!( xh    |xl); }
  if( s>  64 ) { s -= 64; int t = 64-s; *_zh = UINT64_C(0); *_zl =  xh>>s;          return !!((xh<<t)|xl); }
  if( s== 64 ) {                        *_zh = UINT64_C(0); *_zl =  xh;             return !!         xl;  }
  if( s>   0 ) {          int t = 64-s; *_zh = xh>>s;       *_zl = (xl>>s)|(xh<<t); return !!( xl<<t    ); }
  /*  s==  0 */                         *_zh = xh;          *_zl =  xl;             return 0;
}

#ifdef __cplusplus
}
#endif

#endif /* _pyth_oracle_util_uwide_h_ */
