#ifndef _pyth_oracle_model_weight_model_h_
#define _pyth_oracle_model_weight_model_h_

/* Given our estimate of the distribution of prices gap blocks ago and
   an estimate of the current price distribution ("OBS"), compute our
   belief that the two estimates are modeling statistically similar
   price distributions ("SIM").  Via Bayes theorem then:

     PROB(SIM;OBS) = PROB(OBS;SIM) PROB(SIM) / PROB(OBS)
                   = PROB(OBS;SIM) PROB(SIM) / ( PROB(OBS;SIM) PROB(SIM) + PROB(OBS;~SIM) PROB(~SIM) )
                   = R / ( 1 + R )

   where:

     R = ( PROB(OBS;SIM)/PROB(OBS;~SIM) ) ( PROB(SIM)/PROB(~SIM) )

   The first term in R is exactly what overlap_model estimates.  The
   second term is exactly what gap_model estimates.  Specifically,
   weight_model computes:

      weight/2^30 ~ PROB(SIM;OBS)

   given the two ratios

      ratio_overlap/2^30 ~ PROB(OBS;SIM) / PROB(OBS;~SIM)
      ratio_gap    /2^30 ~ PROB(SIM)     / PROB(~SIM)

   This calcuation ~30-bits accurate (the result is +/-1 ulp of the
   correctly rounded result).

   If WEIGHT_MODEL_NEED_REF is defined, this will also declare a high
   accuracy floating point reference implementation "weight_model_ref"
   to aid in testing / tuning block chain suitable approximations. */

#include "../util/uwide.h"

#ifdef WEIGHT_MODEL_NEED_REF
#include <math.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

#ifdef WEIGHT_MODEL_NEED_REF

static long double
weight_model_ref( long double ratio_gap,
                  long double ratio_overlap ) {
  long double r = ratio_gap*ratio_overlap;
  return r / (1.L + r);
}

#endif

static inline uint64_t /* In [0,2^30] */
weight_model( uint64_t ratio_gap,
              uint64_t ratio_overlap ) {
  uint64_t tmp_h,tmp_l;

  /* Starting from the exact expression:

       weight/2^30 = ((ratio_gap/2^30) (ratio_overlap/2^30)) / ( 1 + ((ratio_gap/2^30) (ratio_overlap/2^30)))

     we have:

       weight = 2^30 ratio_gap ratio_overlap / ( 2^60 + ratio_gap ratio_overlap )

     When ratio_gap ratio_overlap <= 2^59/2^30 - 0.5 (>= 2^91-2^60)
     weight rounds to 0 (2^30).  This implies we don't actually need to
     do a division with a 128-bit denominator to get a practically full
     precision result.  Instead we compute:

       ratio_gap ratio_overlap / 2^s

     where s the smallest integer that keep the denominator in 63-bits
     for all non-trivial values of ratio_gap ratio_overlap (63 is used
     instead of 64 to with the rounding calc).  Given the above, s=28 is
     sufficient, yielding:

       weight = 2^2 ratio_gap ratio_overlap / ( 2^32 + (ratio_gap ratio_overlap/2^28) )
       
     The below approximates this via:

       num    = ratio_gap ratio_overlap
       den    = 2^32 + floor( (num + 2^27)/2^28 )
       weight = floor( (2^3 num + den) / (2 den) )

     Should have a maximum error of O(1) ulp. */

  /* Compute num. 128-bit wide needed because these ratios can both
     potentially be >=2^32. */

  uint64_t num_h,num_l; uwide_mul( &num_h,&num_l, ratio_gap, ratio_overlap );

  /* Compute den.  Note that as ratio_gap ratio_overlap is at most
     2^128-2^65+1, the rounding add by 2^27 will not overflow.  If the
     result here >= (2^91-2^60)/2^28 = 2^63-2^32, we know the result
     rounds to 2^30 and return immediately. */

  static uint64_t const thresh = (UINT64_C(1)<<63) - (UINT64_C(1)<<32);

  uwide_add( &tmp_h,&tmp_l, num_h,num_l, UINT64_C(0),UINT64_C(0), UINT64_C(1)<<27 );
  uwide_sr ( &tmp_h,&tmp_l, tmp_h,tmp_l, 28 );
  if( tmp_h || tmp_l>=thresh ) return UINT64_C(1)<<30;
  uint64_t den = (UINT64_C(1)<<32) + tmp_l;

  /* Compute weight.  Note that at this point <num_h,num_l> < 2^91-2^60
     and den < 2^63 such that none of the below can overflow. */

  uwide_sl ( &tmp_h,&tmp_l, num_h,num_l, 3 );
  uwide_add( &tmp_h,&tmp_l, tmp_h,tmp_l, UINT64_C(0),UINT64_C(0), den );
  uwide_div( &tmp_h,&tmp_l, tmp_h,tmp_l, den<<1 );
  uint64_t weight = tmp_l; /* tmp_h==0 at this point */

  return weight;
}

#ifdef __cplusplus
}
#endif

#endif /* _pyth_oracle_model_weight_model_h_ */

