#ifndef _pyth_oracle_util_sqrt_h_
#define _pyth_oracle_util_sqrt_h_

/* Portable robust integer sqrt */

#include "log2.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Compute y = floor( sqrt( x ) ) for unsigned integers exactly.  This
   is based on the fixed point iteration:

     y' = (y + x/y) / 2

   In continuum math, this converges quadratically to sqrt(x).  This is
   a useful starting point for a method because we have a relatively low
   cost unsigned integer division in the machine model and the
   operations and intermediates in this calculation all have magnitudes
   smaller than x (so limited concern about overflow issues).

   We don't do this iteration in integer arithmetic directly because the
   iteration has two roundoff errors while the actual result only has
   one (the floor of the continuum value).  As such, even if it did
   converge in integer arithmetic, probably would not always converge
   exactly.

   We instead combine the two divisions into one, yielding single round
   off error iteration:

     y' = floor( (y^2 + x) / (2 y) )

   As y has about half the width of x given a good initial guess, 2 y
   will not overflow and y^2 + x will be ~2 x and thus any potential
   intermediate overflow issues are cheap to handle.  If this converges,
   at convergence:

        y = (y^2 + x - r) / (2 y)
     -> 2 y^2 = y^2 + x - r
     -> y^2 = x - r

   for some r in [0,2 y-1].  We note that if y = floor( sqrt( x ) )
   exactly though:

        y^2 <= x < (y+1)^2
     -> y^2 <= x < y^2 + 2 y + 1
     -> y^2  = x - r'

   for some r' in [0,2 y].  r' is r with the element 2 y added.  And it
   is possible to have y^2 = x - 2 y.  Namely if x+1 = z^2 for integer
   z, this becomes y^2 + 2 y + 1 = z^2 -> y = z-1.  That is, when
   x = z^2 - 1 for integral z, the relationship can never converge.  If
   we instead used a denominator of 2y+1 in the iteration, r would have
   the necessary range:

     y' = floor( (y^2 + x) / (2 y + 1) )

   At convergence we have:

        y = (y^2 + x - r) / (2 y+1)
     -> 2 y^2 + y = (y^2 + x - r)
     -> y^2 = x-y-r

   for some r in [0,2 y].  This isn't quite right but we change the
   recurrence numerator to compensate:

     y' = floor( (y^2 + y + x) / (2 y + 1) )
  
   At convergence we now have:

     y^2 = x-r

   for some r in [0,2 y].  That is, at convergence y = floor( sqrt(x) )
   exactly!  The addition of y to the numerator has not made
   intermediate overflow much more diffcult to deal with either as y <<<
   x for large x.  So to compute this without intermediate overflow, we
   compute the terms individually and then combine the reminders
   appropriately.  x/(2y+1) term is trivial.  The other term,
   (y^2+y)/(2y+1) is asymptotically approximately y/2.  Breaking it into
   its asymptotic and residual:

      (y^2 + y) / (2y+1) = y/2 + ( y^2 + y - (y/2)(2y+1) ) / (2y+1)
                         = y/2 + ( y^2 + y - y^2 - y/2   ) / (2y+1)
                         = y/2 + (y/2) / (2y+1)

   For even y, y/2 = y>>1 = yh and we have the partial quotient yh and
   remainder yh.  For odd y, we have:

                         = yh + (1/2) + (yh+(1/2)) / (2y+1)
                         = yh + ((1/2)(2y+1)+yh+(1/2)) / (2y+1)
                         = yh + (y+yh+1) / (2y+1)

   with partial quotent yh and remainder y+yh+1.  This yields the
   iteration:

     y ~ sqrt(x)                               // <<< INT_MAX for all x
     for(;;) {
       d  = 2*y + 1;
       qx = x / d; rx = x - qx*d;              // Compute x  /(2y+1), rx in [0,2y]
       qy = y>>1;  ry = (y&1) ? (y+yh+1) : yh; // Compute y^2/(2y+1), ry in [0,2y]
       q  = qx+qy; r  = rx+ry;                 // Combine partials, r in [0,4y]
       if( r>=d ) q++, r-=d;                   // Handle carry (at most 1), r in [0,2y]
       if( y==q ) break;                       // At convergence y = floor(sqrt(x))
       y = q;
     }

   The better the initial guess, the faster this will converge.  Since
   convergence is still quadratic though, it will converge even given
   very simple guesses.  We use:

     y = sqrt(x) = sqrt( 2^n + d ) <~ 2^(n/2)
   
   where n is the index of the MSB and d is in [0,2^n) (i.e. is n bits
   wide).  Thus:

     y ~ 2^(n>>1) if n is even and 2^(n>>1) sqrt(2) if n is odd

   and we can do a simple fixed point calculation to compute this.

   For small values of x, we encode a 20 entry 3-bit wide lookup table
   in a 64-bit constant and just do a quick lookup. */

static inline uint8_t
sqrt_uint8( uint8_t x ) { /* FIXME: CONSIDER 8-bit->4-bit LUT INSTEAD (128bytes)? */
  if( x<UINT8_C(21) ) return (uint8_t)((UINT64_C(0x49246db6da492248) >> (3*(int)x)) & UINT64_C(7));
  int     n = log2_uint8( x );
  uint8_t y = (uint8_t)(((n & 1) ? UINT8_C(0xb) /* floor( 2^3 sqrt(2) ) */ : UINT8_C(0x8) /* 2^3 */) >> (3-(n>>1)));
  for(;;) {
    uint8_t d = (uint8_t)(y<<1); d++;
    uint8_t qx = x / d;            uint8_t rx = (uint8_t)(x - qx*d);
    uint8_t qy = y>>1;             uint8_t ry = (y & UINT8_C(1)) ? (uint8_t)(y+qy+UINT8_C(1)) : qy;
    uint8_t q  = (uint8_t)(qx+qy); uint8_t r  = (uint8_t)(rx+ry);
    if( r>=d ) q++;
    if( y==q ) break;
    y = q;
  }
  return y;
}

static inline uint16_t
sqrt_uint16( uint16_t x ) {
  if( x<UINT16_C(21) ) return (uint16_t)((UINT64_C(0x49246db6da492248) >> (3*(int)x)) & UINT64_C(7));
  int      n = log2_uint16( x );
  uint16_t y = (uint16_t)(((n & 1) ? UINT16_C(0xb5) /* floor( 2^7 sqrt(2) ) */ : UINT16_C(0x80) /* 2^7 */) >> (7-(n>>1)));
  for(;;) {
    uint16_t d = (uint16_t)(y<<1); d++;
    uint16_t qx = x / d;             uint16_t rx = (uint16_t)(x - qx*d);
    uint16_t qy = y>>1;              uint16_t ry = (y & UINT16_C(1)) ? (uint16_t)(y+qy+UINT16_C(1)) : qy;
    uint16_t q  = (uint16_t)(qx+qy); uint16_t r  = (uint16_t)(rx+ry);
    if( r>=d ) q++;
    if( y==q ) break;
    y = q;
  }
  return y;
}

static inline uint32_t
sqrt_uint32( uint32_t x ) {
  if( x<UINT32_C(21) ) return (uint32_t)((UINT64_C(0x49246db6da492248) >> (3*(int)x)) & UINT64_C(7));
  if( !x ) return UINT32_C(0);
  int      n = log2_uint32( x ); /* In [0,63) */
  uint32_t y = ((n & 1) ? UINT32_C(0xb504) /* floor( 2^15 sqrt(2) ) */ : UINT32_C(0x8000) /* 2^15 */) >> (15-(n>>1));
  for(;;) {
    uint32_t d = (y<<1); d++;
    uint32_t qx = x / d; uint32_t rx = x - qx*d;
    uint32_t qy = y>>1;  uint32_t ry = (y & UINT32_C(1)) ? (y+qy+UINT32_C(1)) : qy;
    uint32_t q  = qx+qy; uint32_t r  = rx+ry;
    if( r>=d ) q++;
    if( y==q ) break;
    y = q;
  }
  return y;
}

static inline uint64_t
sqrt_uint64( uint64_t x ) {
  if( x<UINT64_C(21) ) return (UINT64_C(0x49246db6da492248) >> (3*(int)x)) & UINT64_C(7);
  int      n = log2_uint64( x ); /* In [0,63) */
  uint64_t y = ((n & 1) ? UINT64_C(0xb504f333) /* floor( 2^31 sqrt(2) ) */ : UINT64_C(0x80000000) /* 2^31 */) >> (31-(n>>1));
  for(;;) {
    uint64_t d = (y<<1); d++;
    uint64_t qx = x / d; uint64_t rx = x - qx*d;
    uint64_t qy = y>>1;  uint64_t ry = (y & UINT64_C(1)) ? (y+qy+UINT64_C(1)) : qy;
    uint64_t q  = qx+qy; uint64_t r  = rx+ry;
    if( r>=d ) q++;
    if( y==q ) break;
    y = q;
  }
  return y;
}

/* These return floor( sqrt( x ) ), undefined behavior for negative x. */

static inline int8_t  sqrt_int8 ( int8_t  x ) { return (int8_t )sqrt_uint8 ( (uint8_t) x ); }
static inline int16_t sqrt_int16( int16_t x ) { return (int16_t)sqrt_uint16( (uint16_t)x ); }
static inline int32_t sqrt_int32( int32_t x ) { return (int32_t)sqrt_uint32( (uint32_t)x ); }
static inline int64_t sqrt_int64( int64_t x ) { return (int64_t)sqrt_uint64( (uint64_t)x ); }

/* These return the floor( re sqrt(x) ) */

static inline int8_t  sqrt_re_int8 ( int8_t  x ) { return x>INT8_C( 0) ? (int8_t )sqrt_uint8 ( (uint8_t) x ) : INT8_C( 0); }
static inline int16_t sqrt_re_int16( int16_t x ) { return x>INT16_C(0) ? (int16_t)sqrt_uint16( (uint16_t)x ) : INT16_C(0); }
static inline int32_t sqrt_re_int32( int32_t x ) { return x>INT32_C(0) ? (int32_t)sqrt_uint32( (uint32_t)x ) : INT32_C(0); }
static inline int64_t sqrt_re_int64( int64_t x ) { return x>INT64_C(0) ? (int64_t)sqrt_uint64( (uint64_t)x ) : INT64_C(0); }

/* These return the floor( sqrt( |x| ) ) */

static inline int8_t  sqrt_abs_int8 ( int8_t  x ) { return (int8_t )sqrt_uint8 ( (uint8_t) (x<INT8_C( 0) ? -x : x) ); }
static inline int16_t sqrt_abs_int16( int16_t x ) { return (int16_t)sqrt_uint16( (uint16_t)(x<INT16_C(0) ? -x : x) ); }
static inline int32_t sqrt_abs_int32( int32_t x ) { return (int32_t)sqrt_uint32( (uint32_t)(x<INT32_C(0) ? -x : x) ); }
static inline int64_t sqrt_abs_int64( int64_t x ) { return (int64_t)sqrt_uint64( (uint64_t)(x<INT64_C(0) ? -x : x) ); }

#ifdef __cplusplus
}
#endif

#endif /* _pyth_oracle_util_sqrt_h_ */

