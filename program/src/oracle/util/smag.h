#ifndef _pyth_oracle_util_smag_h_
#define _pyth_oracle_util_smag_h_

/* Signed magnitude conversions to/from twos complement
   representations. */

#include "compat_stdint.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Unpack a twos complement represention into signed magnitude
   representation.  Returns magnitude, assumes _s is non-NULL and *_s
   will be 0 or 1 on return.  For the 32-bit variant, return will be in
   [0,2^31] and *_s==1 if the return is 2^31.  Similarly for the other
   widths. */

static inline uint8_t  smag_unpack_int8 ( int8_t  x, int * _s ) { int s = x<INT8_C( 0); *_s = s; return (uint8_t )( s ? -x : x ); }
static inline uint16_t smag_unpack_int16( int16_t x, int * _s ) { int s = x<INT16_C(0); *_s = s; return (uint16_t)( s ? -x : x ); }
static inline uint32_t smag_unpack_int32( int32_t x, int * _s ) { int s = x<INT32_C(0); *_s = s; return (uint32_t)( s ? -x : x ); }
static inline uint64_t smag_unpack_int64( int64_t x, int * _s ) { int s = x<INT64_C(0); *_s = s; return (uint64_t)( s ? -x : x ); }

/* Pack signed magnitude representation into twos complement
   representation (roughly speaking compute (-1)^s m).  For the 32-bit
   variant, assumes m<2^31 or (m==2^31 and s!=0).  Similarly for the
   other widths. */

static inline int8_t  smag_pack_int8 ( int s, uint8_t  m ) { return (int8_t )( s ? -m : m ); }
static inline int16_t smag_pack_int16( int s, uint16_t m ) { return (int16_t)( s ? -m : m ); }
static inline int32_t smag_pack_int32( int s, uint32_t m ) { return (int32_t)( s ? -m : m ); }
static inline int64_t smag_pack_int64( int s, uint64_t m ) { return (int64_t)( s ? -m : m ); }

/* FIXME: CONSIDER MOVING SIGNBIT AND ABS TO THEIR OWN HEADERS */

static inline int signbit_int8 ( int8_t  x ) { return x<INT8_C( 0); }
static inline int signbit_int16( int16_t x ) { return x<INT16_C(0); }
static inline int signbit_int32( int32_t x ) { return x<INT32_C(0); }
static inline int signbit_int64( int64_t x ) { return x<INT64_C(0); }

static inline uint8_t  abs_int8 ( int8_t  x ) { return (uint8_t )( x<INT8_C( 0) ? -x : x ); }
static inline uint16_t abs_int16( int16_t x ) { return (uint16_t)( x<INT16_C(0) ? -x : x ); }
static inline uint32_t abs_int32( int32_t x ) { return (uint32_t)( x<INT32_C(0) ? -x : x ); }
static inline uint64_t abs_int64( int64_t x ) { return (uint64_t)( x<INT64_C(0) ? -x : x ); }

/* FIXME: CONSIDER FOR API COMPLETENESS/CONSISTENCY UINT VARIANTS (THEY
   ARE NOPS) */

#ifdef __cplusplus
}
#endif

#endif /* _pyth_oracle_util_smag_h_ */

