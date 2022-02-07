#ifndef _pyth_oracle_model_leaky_integrator_h_
#define _pyth_oracle_model_leaky_integrator_h_

/* Compute:

     (z/2^30) ~ (y/2^30)(w/2^30) + x/2^30

   Specifically returns:

     co 2^128 + <zh,zl> = round( <yh,yl> w / 2^30 ) + x

   where co is the "carry out".  (Can be used to detect overflow and/or
   automatically rescale as necessary.)  Assumes w is in [0,2^30].
   Calculation is essentially full precision O(1) and more accurate than
   a long double implementation for a wide range inputs. */

#include "../util/uwide.h"

#ifdef __cplusplus
extern "C" {
#endif

static inline uint64_t
leaky_integrator( uint64_t * _zh, uint64_t * _zl,
                  uint64_t    yh, uint64_t    yl,
                  uint64_t    w,    /* In [0,2^30] */
                  uint64_t    x ) {

  /* Start from the exact expression:

       z/2^30 = (y/2^30)*(w/2^30) + x/2^30

     This yields:

       z = (y w)/2^30 + x
       z = (yh w 2^64 + yl w)/2^30 + x
         = yh w 2^34 + yl w/2^30 + x
         ~ yh w 2^34 + floor((yl w + 2^29)/2^30) + x */

  uint64_t t0_h,t0_l; uwide_mul( &t0_h,&t0_l, yh, w ); /* At most 2^94-2^30 */
  uwide_sl( &t0_h,&t0_l, t0_h,t0_l, 34 );              /* At most 2^128-2^64 (so cannot overflow) */

  uint64_t t1_h,t1_l; uwide_mul( &t1_h,&t1_l, yl, w );                           /* At most 2^94-2^30 */
  uwide_add( &t1_h,&t1_l, t1_h,t1_l, UINT64_C(0),UINT64_C(0), UINT64_C(1)<<29 ); /* At most 2^94-2^29 (all ones 30:93) */
  uwide_sr ( &t1_h,&t1_l, t1_h,t1_l, 30 );                                       /* At most 2^64-1 (all ones 0:63) */

  return uwide_add( _zh,_zl, t0_h,t0_l, t1_h,t1_l, x );
}

#ifdef __cplusplus
}
#endif

#endif /* _pyth_oracle_model_leaky_integrator_h_ */

