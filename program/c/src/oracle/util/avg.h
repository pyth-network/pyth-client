#ifndef _pyth_oracle_util_avg_h_
#define _pyth_oracle_util_avg_h_

/* Portable robust integer averaging.  (No intermediate overflow, no
   assumptions about platform type wides, no assumptions about integer
   signed right shift, no signed integer division, no implict
   conversions, etc.) */

#include "sar.h"

#ifdef __cplusplus
extern "C" {
#endif

/* uint8_t  a = avg_2_uint8 ( x, y ); int8_t  a = avg_2_int8 ( x, y );
   uint16_t a = avg_2_uint16( x, y ); int16_t a = avg_2_int16( x, y );
   uint32_t a = avg_2_uint32( x, y ); int32_t a = avg_2_int32( x, y );
   uint64_t a = avg_2_uint64( x, y ); int64_t a = avg_2_int64( x, y );

   compute the average (a) of x and y of the corresponding type with <1
   ulp error (floor / round toward negative infinity rounding).  That
   is, these compute this exactly:
     floor( (x+y)/2 )
   On systems where signed right shifts are sign extending, this is
   identical to:
     (x+y) >> 1
   but the below will be safe against intermediate overflow. */

static inline uint8_t  avg_2_uint8 ( uint8_t  x, uint8_t  y ) { return (uint8_t )((((uint16_t)x)+((uint16_t)y))>>1); }
static inline uint16_t avg_2_uint16( uint16_t x, uint16_t y ) { return (uint16_t)((((uint32_t)x)+((uint32_t)y))>>1); }
static inline uint32_t avg_2_uint32( uint32_t x, uint32_t y ) { return (uint32_t)((((uint64_t)x)+((uint64_t)y))>>1); }

static inline uint64_t
avg_2_uint64( uint64_t x,
              uint64_t y ) {

  /* x+y might have intermediate overflow and we don't have an efficient
     portable wider integer type available.  So we can't just do
     (x+y)>>1 to compute floor((x+y)/2). */

# if 1 /* Fewer ops but less parallel issue */
  /* Emulate an adc (add with carry) to get the 65-bit wide intermediate
     and then shift back in as necessary */
  uint64_t z = x + y;
  uint64_t c = (uint64_t)(z<x); // Or z<y
  return (z>>1) + (c<<63);
# else /* More ops but more parallel issue */
  /* floor(x/2)+floor(y/2)==(x>>1)+(y>>1) doesn't have intermediate
     overflow but it has two rounding errors.  Noting that
       (x+y)/2 == floor(x/2) + floor(y/2) + (x_lsb+y_lsb)/2
               == (x>>1)     + (y>>1)     + (x_lsb+y_lsb)/2
     implies we should increment when x and y have their lsbs set. */
  return (x>>1) + (y>>1) + (x & y & UINT64_C(1));
# endif

}

static inline int8_t  avg_2_int8  ( int8_t  x, int8_t  y ) { return (int8_t )sar_int16((int16_t)(((int16_t)x)+((int16_t)y)),1); }
static inline int16_t avg_2_int16 ( int16_t x, int16_t y ) { return (int16_t)sar_int32((int32_t)(((int32_t)x)+((int32_t)y)),1); }
static inline int32_t avg_2_int32 ( int32_t x, int32_t y ) { return (int32_t)sar_int64((int64_t)(((int64_t)x)+((int64_t)y)),1); }

static inline int64_t
avg_2_int64( int64_t x,
             int64_t y ) {

  /* Similar considerations as above */

# if 1 /* Fewer ops but less parallel issue */
  /* x+y = x+2^63 + y+2^63 - 2^64 ... exact ops
         = ux     + uy     - 2^64 ... exact ops where ux and uy are exactly representable as a 64-bit uint
         = uz     + 2^64 c - 2^64 ... exact ops where uz is a 64-bit uint and c is in [0,1].
     Thus, as before
       uz = ux+uy   ... c ops
       c  = (uz<ux) ... exact ops
     Further simplifying:
       x+y = uz - 2^64 ( 1-c ) ... exact ops
           = uz - 2^64 b       ... exact ops
     where:
       b  = 1-c = ~c = (uz>=ux) ... exact ops
     Noting that (ux + uy) mod 2^64 = (x+y+2^64) mod 2^64 = (x+y) mod 2^64
     and that signed and added adds are the same operation binary in twos
     complement math yields:
       uz  = (uint64_t)(x+y) ... c ops
       b   = uz>=(x+2^63)    ... exact ops
     as we know x+2^63 is in [0,2^64), x+2^63 = x+/-2^63 mod 2^64.  And
     using the signed and unsigned adds are the same operation binary
     in we have:
       x+2^63              ... exact ops
       ==x+/-2^63 mod 2^64 ... exact ops
       ==x+/-2^63          ... c ops  */
   uint64_t t = (uint64_t)x;
   uint64_t z = t + (uint64_t)y;
   uint64_t b = (uint64_t)(z>=(t+(((uint64_t)1)<<63)));
   return (int64_t)((z>>1) - (b<<63));
# else /* More ops but more parallel issue (significant more ops if no platform arithmetic right shift) */
  /* See above for uint64_t for details on this calc. */
  return sar_int64( x, 1 ) + sar_int64( y, 1 ) + (x & y & (int64_t)1);
# endif

}

/* uint8_t  a = avg_uint8 ( x, n ); int8_t  a = avg_int8 ( x, n );
   uint16_t a = avg_uint16( x, n ); int16_t a = avg_int16( x, n );
   uint32_t a = avg_uint32( x, n ); int32_t a = avg_int32( x, n );

   compute the average (a) of the n element array f the corresponding
   type pointed to by x with <1 ulp round error (truncate / round toward
   zero rounding).  Supports arrays with up to 2^32-1 elements.  Returns
   0 for arrays with 0 elements.  Elements of the array are unchanged by
   this function.  No 64-bit equivalents are provided here as that
   requires a different and slower implementation (at least if
   portability is required). */

#define PYTH_ORACLE_UTIL_AVG_DECL(func,T) /* Comments for uint32_t but apply for uint8_t and uint16_t too */ \
static inline T       /* == (1/n) sum_i x[i] with one rounding error and round toward zero rounding */       \
func( T const * x,    /* Indexed [0,n) */                                                                    \
      uint32_t  n ) { /* Returns 0 on n==0 */                                                                \
  if( !n ) return (T)0; /* Handle n==0 case */                                                               \
  /* At this point n is in [1,2^32-1].  Sum up all the x into a 64-bit */                                    \
  /* wide signed accumulator. */                                                                             \
  uint64_t a = (uint64_t)0;                                                                                  \
  for( uint32_t r=n; r; r-- ) a += (uint64_t)*(x++);                                                         \
  /* At this point a is in [ 0, (2^32-1)(2^32-1) ] < 2^64 and thus was */                                    \
  /* computed without intermediate overflow.  Complete the average.    */                                    \
  return (T)( a / (uint64_t)n );                                                                             \
}

PYTH_ORACLE_UTIL_AVG_DECL( avg_uint8,  uint8_t  )
PYTH_ORACLE_UTIL_AVG_DECL( avg_uint16, uint16_t )
PYTH_ORACLE_UTIL_AVG_DECL( avg_uint32, uint32_t )
/* See note above about 64-bit variants */

#undef PYTH_ORACLE_UTIL_AVG_DECL

#define PYTH_ORACLE_UTIL_AVG_DECL(func,T) /* Comments for int32_t but apply for int8_t and int16_t too */ \
static inline T       /* == (1/n) sum_i x[i] with one rounding error and round toward zero rounding */    \
func( T const * x,    /* Indexed [0,n) */                                                                 \
      uint32_t  n ) { /* Returns 0 on n==0 */                                                             \
  if( !n ) return (T)0; /* Handle n==0 case */                                                            \
  /* At this point n is in [1,2^32-1].  Sum up all the x into a 64-bit */                                 \
  /* wide signed accumulator. */                                                                          \
  int64_t a = (int64_t)0;                                                                                 \
  for( uint32_t r=n; r; r-- ) a += (int64_t)*(x++);                                                       \
  /* At this point a is in [ -2^31 (2^32-1), (2^31-1)(2^32-1) ] such that */                              \
  /* |a| < 2^63.  It thus was computed without intermediate overflow and  */                              \
  /* further |a| can fit with an uint64_t.  Compute |a|/n.  If a<0, the   */                              \
  /* result will be at most 2^31-1 (i.e. all x[i]==2^31-1).  Otherwise,   */                              \
  /* the result will be at most 2^31 (i.e. all x[i]==-2^31). */                                           \
  int      s = a<(int64_t)0;                                                                              \
  uint64_t u = (uint64_t)(s ? -a : a);                                                                    \
  u /= (uint64_t)n;                                                                                       \
  a = (int64_t)u;                                                                                         \
  return (T)(s ? -a : a);                                                                                 \
}

PYTH_ORACLE_UTIL_AVG_DECL( avg_int8,  int8_t  )
PYTH_ORACLE_UTIL_AVG_DECL( avg_int16, int16_t )
PYTH_ORACLE_UTIL_AVG_DECL( avg_int32, int32_t )
/* See note above about 64-bit variants */

#undef PYTH_ORACLE_UTIL_AVG_DECL

#ifdef __cplusplus
}
#endif

#endif /* _pyth_oracle_util_avg_h_ */
