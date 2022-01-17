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

/* Compute co 2^128 + <zh,zl> = <xh,xl> + <yh,yl> + ci exactly.  Returns
   the carry out.  Note that the carry in / carry operations should be
   compile time optimized out if ci is 0 at compile time on input and/or
   return value is not used. */

static inline uint64_t
uwide_add( uint64_t * _zh, uint64_t * _zl,
           uint64_t    xh, uint64_t    xl,
           uint64_t    yh, uint64_t    yl,
           uint64_t    ci ) {
  uint64_t zh = xh; uint64_t zl = xl;
  uint64_t ct;      uint64_t co;
  zl += ci; ct  = (uint64_t)(zl<ci);
  zl += yl; ct += (uint64_t)(zl<yl);
  zh += ct; co  = (uint64_t)(zh<ct);
  zh += yh; co += (uint64_t)(zh<yh);
  *_zh = zh; *_zl = zl; return co;
}

/* Compute <zh,zl> = bo 2^128 + <xh,xl> - <yh,yl> - bi exactly.  Returns
   the borrow out.  Note that the borrow in / borrow operations should
   be compile time optimized out if b is 0 at compile time on input
   and/or return value is not used. */

static inline uint64_t
uwide_sub( uint64_t * _zh, uint64_t * _zl,
           uint64_t    xh, uint64_t    xl,
           uint64_t    yh, uint64_t    yl,
           uint64_t    bi ) {
  uint64_t zh = xh; uint64_t zl = xl;
  uint64_t bt;      uint64_t bo;
  bt  = (uint64_t)(zl<bi); zl -= bi;
  bt += (uint64_t)(zl<yl); zl -= yl;
  bo  = (uint64_t)(zh<bt); zh -= bt;
  bo += (uint64_t)(zh<yh); zh -= yh;
  *_zh = zh; *_zl = zl; return bo;
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

  *_zh = zh; *_zl = zl;
}

/* Compute n = floor( log2 <xh,xl> ) exactly.  Assumes <xh,xl> is not 0. */

static inline int
uwide_log2( uint64_t xh,
            uint64_t xl ) {
  int off = 0;
  if( xh ) { off = 64; xl = xh; }
  return off + log2_uint64( xl );
}

/* Same as the above but returns def is <xh,xl> is 0. */

static inline int uwide_log2_def( uint64_t xh, uint64_t xl, int def ) { return (xh|xl) ? uwide_log2(xh,xl) : def; }

/* Compute <zh,zl> = <xh,xl> << s.  Returns the overflow flag (will be 0
   or 1) which indicates if any non-zero bits of <xh,xl> were lost in
   the process.  Note that the overflow and various cases should be
   compile time optimized out if if s is known at compile time on input
   and/or return value is not used.  Assumes s is non-negative (FIXME:
   CONSIDER HAVING AN INVALID OP FLAG TOO?) . */

static inline int
uwide_sl( uint64_t * _zh, uint64_t * _zl,
          uint64_t    xh, uint64_t    xl,
          int s ) {
  if( s>=128 ) {                        *_zh = UINT64_C(0);     *_zl = UINT64_C(0); return !!(xh| xl    ); }
  if( s>  64 ) { s -= 64; int t = 64-s; *_zh =  xl<<s;          *_zl = UINT64_C(0); return !!(xh|(xl>>t)); }
  if( s== 64 ) {                        *_zh =  xl;             *_zl = UINT64_C(0); return !! xh;          }
  if( s>   0 ) {          int t = 64-s; *_zh = (xh<<s)|(xl>>t); *_zl = xl<<s;       return !!(xh>>t);      }
  /*  s==  0 */                         *_zh =  xh;             *_zl = xl;          return 0;
}

/* Compute <zh,zl> = <xh,xl> >> s.  Returns the underflow flag (will be
   0 or 1) which indicates if any non-zero bits of xh and xl were lost
   in process.  Note that the underflow and various cases should be
   compile time optimized out if if s is known at compile time on input
   and/or return value is not used.  (FIXME: CONSIDER HAVING AN INVALID
   OP FLAGS AND MORE DETAILED UNDERFLOW FLAG TO SIMPLIFY ROUNDING MODE
   SUPPORT?) */

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

/* Compute a ~32-bit accurate approximation of q = floor(n 2^64 / d)
   where d is in [2^63,2^64).  Setup cost for the first n is
   approximately a single 64/64->64 integer divide.  Cost per n
   afterwards is approximately 2 32*32->64 multiplications.
   Approximation will <=q.

   Theory: Let q d + r = n 2^64 where q = floor( n 2^64 / d ).  Note r
   is in [0,d).  Break d into d = dh 2^32 + dl where dh and dl are
   32-bit words:
     q+r/d = n 2^64 / d =  n 2^64 / ( dh 2^32 + dl )
                        =  n 2^64 / ( (dh+1) 2^32 - (2^32-dl) ) ... dh+1 can be computed without overflow, 2^32-dl is positive
                        >  n 2^64 / ( (dh+1) 2^32 )
                        >= n floor( 2^64/(dh+1) ) / 2^32
   Note that floor( 2^64/(dh+1) ) is in [2^32,2^33-4].  This suggests
   letting:
     2^32 + m = floor( 2^64/(dh+1) )
   where m is in [0,2^32-4] (a uint32_t).  Then we have:
     q+r/d > n (2^32 + m) / 2^32
           = n + n m / 2^32
   Similarly breaking n into n = nh 2^32 + nl:
     q+r/d > n + (nh m) + (nl m/2^32)
           >= n + nh m + floor( nl m / 2^32 )
   We get:
     q+r/d > n + nh*m + ((nl*m)>>32)
   And, as q is integral, r/d is less than 1 and the right hand side is
   integral:
     q >= qa
   where:
     qa = n + nh*m + (((nl*m)>>32)
   and:
     m  = floor( 2^64 / (dh+1) ) - 2^32

   To compute m efficiently, note:
     m  = floor( 1 + (2^64-(dh+1))/(dh+1) ) - 2^32
        = floor( (2^64-(dh+1))/(dh+1) ) - (2^32-1)
   and in "C style" modulo 2^64 arithmetic, 2^64 - x = -x.  This yields:
     m  = (-(dh+1))/(dh+1) - (2^32-1)

   In the general case, qa is up to 65 bits wide (worst case is n=2^64-1
   and d=2^63 such that q is 2^65-2 and qa is precise enough in this
   case as to need 65 bits too).  The implementation here assumes n is
   is less than d here so that q and qa are both known to fit within 64
   bits. */

static inline uint64_t                    /* In [0,2^32) */
uwide_div_approx_init( uint64_t d ) {     /* d in [2^63,2^64) */
  uint64_t m = (d>>32) + UINT64_C(1);     /* m = dh+1 and is in (2^31,2^32]  ... exact */
  return ((-m)/m) - (uint64_t)UINT32_MAX; /* m = floor( 2^64/(dh+1) ) - 2^32 ... exact */
}

static inline uint64_t           /* In [n,2^64) */
uwide_div_approx( uint64_t n,    /* In [0,d) */
                  uint64_t m ) { /* Output of uwide_div_helper_init for the desired d in [2^63,2^64) */
  uint64_t nh = n>>32;
  uint64_t nl = (uint64_t)(uint32_t)n;
  return n + nh*m + ((nl*m)>>32);
}

/* Compute z = floor( (xh 2^64 + xl) / y ).  Requires y to be non-zero.
   Returns divide by zero flag if y is 0 (<zh,zl>=0 in this case). */

static inline int
uwide_div( uint64_t * _zh, uint64_t * _zl,
           uint64_t    xh, uint64_t    xl,
           uint64_t    y ) {

  /* Simple cases (y==0, x==0 and y==2^n) */

  if( !y ) { *_zh = UINT64_C(0); *_zl = UINT64_C(0); return 1; }
  if( !xh ) { *_zh = UINT64_C(0); *_zl = xl / y; return 0; }

  int n = log2_uint64( y );
  if( !(y & (y-UINT64_C(1))) ) { uwide_sr( _zh,_zl, xh,xl, n ); return 0; }

  /* General case ... compute zh noting that:
       <zh,zl> = floor( (xh 2^64 + xl) / y )
               = floor( ((qh y + rh) 2^64 + xl) / y )
     where xh = qh y + rh and qh is floor(xh/y), rh = xh%y and rh is in
     [0,y).  Thus:
       = qh 2^64 + floor( (rh 2^64 + xl) / y )
     where the right term is less that 2^64 such that zh is qh and
     zl = floor( (rh 2^64 + xl) / y ). */

  uint64_t qh = xh / y;
  uint64_t rh = xh - qh*y;

  /* Short cut the trivial zl case */

  if( !rh ) { *_zh = qh; *_zl = xl / y; return 0; }

  /* At this point, zh is qh and zl is floor( (rh 2^64 + xl) / y ) where
     rh is in (0,y) and y is a positive non-power of 2.

     Normalize this by noting for:
       q=n/d; r=n-q*d=n%d
     we have:
       -> n   = q d + r where q = floor(n/d) and r in [0,d).
       -> n w = q d w + r w for some integer w>0
     That is, if we scale up both n and d by w, it doesn't affect q (it
     just scales the remainder).  We use the scale factor of 2^s on
     <rh,xl> and y such that y will have its most significant bit set.
     This will not cause <rh,xl> to overflow as rh is less than y at
     this point. */

  int s = 63-n;
  uint64_t nh, nl; uwide_sl( &nh,&nl, rh,xl, s );
  uint64_t d = y << s;

  /* At this point zh = qh and zl is floor( (nh 2^64 + nl) / d ) where
     nh is in (0,d) and d is in (2^63,2^64). */

  uint64_t m  = uwide_div_approx_init( d );
  uint64_t eh = nh; uint64_t el = nl;
  uint64_t ql = UINT64_C(0);
  do {

    /* At this point, n-ql*d has an error of <eh,el> such that:
         d < 2^64 <= <eh,el>
       (i.e. eh is non-zero).  If we increment ql by the correction
       floor(<eh,el>/d), we'd be at the solution but computing this is
       as hard as the original problem.  We do have the ability to
       compute a ~32-bit accurate estimate ( <eh,0>/d ) very fast
       though.  So we use that as our increment.  Practically, this loop
       will require at most ~4 iterations. */

    ql += uwide_div_approx( eh, m );                 /* Guaranteed to make progress if eh > 0 */
    uwide_mul( &eh,&el, ql, d );                     /* <eh,el> = ql*d */
    uwide_sub( &eh,&el, nh,nl, eh,el, UINT64_C(0) ); /* <eh,el> = nh 2^64 - ql d */
  } while( eh );

  /* At this point, n - ql*d has an error less than 2^64 so we can
     directly compute the remaining correction. */

  ql += el/d;

  *_zh = qh; *_zl = ql; return 0;
}

#ifdef __cplusplus
}
#endif

#endif /* _pyth_oracle_util_uwide_h_ */
