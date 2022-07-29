#ifndef _pyth_oracle_util_sar_h_
#define _pyth_oracle_util_sar_h_

/* Portable arithmetic shift right. */

#include "compat_stdint.h"

/* Define PYTH_ORACLE_UTIL_SAR_USE_PLATFORM to non-zero if the platform
   does arithmetic right shift signed integer and zero if the platform
   does not or is not known to.  PYTH_ORACLE_UTIL_SAR_USE_PLATFORM
   defaults to zero. */

#ifndef PYTH_ORACLE_UTIL_SAR_USE_PLATFORM
#define PYTH_ORACLE_UTIL_SAR_USE_PLATFORM 0
#endif

#ifdef __cplusplus
extern "C" {
#endif

static inline int8_t
sar_int8( int8_t x,
          int    n ) { /* Assumed in [0,7] */
# if PYTH_ORACLE_UTIL_SAR_USE_PLATFORM
  return x >> n;
# else
  uint8_t ux = (uint8_t)x;
  uint8_t s  = (uint8_t)-(ux >> 7); /* get the sign, ((int8_t)s)==-1 if x<0 and 0 otherwise */
  return (int8_t)(((ux ^ s) >> n) ^ s);
# endif
}

static inline int16_t
sar_int16( int16_t x,
           int     n ) { /* Assumed in [0,15] */
# if PYTH_ORACLE_UTIL_SAR_USE_PLATFORM
  return x >> n;
# else
  uint16_t ux = (uint16_t)x;
  uint16_t s  = (uint16_t)-(ux >> 15); /* get the sign, ((int16_t)s)==-1 if x<0 and 0 otherwise */
  return (int16_t)(((ux ^ s) >> n) ^ s);
# endif
}

static inline int32_t
sar_int32( int32_t x,
           int     n ) { /* Assumed in [0,31] */
# if PYTH_ORACLE_UTIL_SAR_USE_PLATFORM
  return x >> n;
# else
  uint32_t ux = (uint32_t)x;
  uint32_t s  = -(ux >> 31); /* get the sign, ((int32_t)s)==-1 if x<0 and 0 otherwise */
  return (int32_t)(((ux ^ s) >> n) ^ s);
# endif
}

static inline int64_t
sar_int64( int64_t x,
           int     n ) { /* Assumed in [0,63] */
# if PYTH_ORACLE_UTIL_SAR_USE_PLATFORM
  return x >> n;
# else
  uint64_t ux = (uint64_t)x;
  uint64_t s  = -(ux >> 63); /* get the sign, ((int64_t)s)==-1 if x<0 and 0 otherwise */
  return (int64_t)(((ux ^ s) >> n) ^ s);
# endif
}

#ifdef __cplusplus
}
#endif

#endif /* _pyth_oracle_util_sar_h_ */
