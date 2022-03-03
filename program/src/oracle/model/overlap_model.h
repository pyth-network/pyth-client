#ifndef _pyth_oracle_model_overlap_model_h_
#define _pyth_oracle_model_overlap_model_h_

/* overlap_model computes our prior belief whether two
   (agg_price,agg_conf) estimates parameterize the same underlying
   process.  Specifically, let ps / pd be our apriori belief that two
   different price distribution estimates came from the same / different
   underlying process.  Then overlap_model computes:

     r/2^30 ~ ps / pd

   Let Lij be the expected likelihood of IID sample drawn from a
   Gaussian distribution with mean/rms mu_i/sigma_i for a Gaussian
   distribution with mean/rms mu_j/sigma_j.  The implementation below
   corresponds to:

     ps / pd = weight sqrt( (L01 L10) / (L00 L11) )

   After some fun with integrals:

     ps / pd = weight sqrt( 2 sigma_0 sigma_1 / (sigma_0^2 + sigma_1^2) )
                      exp( -0.5 (mu_0-mu_1)^2 / (sigma_0^2 + sigma_1^2) )

   where the estimates to compare are (mu0,sigma0) and (mu1,sigma1) and
   weight is the ps / pd prior to use for identical estimates (e.g.
   what our prior belief that two similar agg_price,agg_conf estimates
   just happen to be parameterizing different processes with similar
   distributions).  As such the output is in [0,weight 2^30].  This
   model is heuristic but well motivated theoretically with good
   limiting behaviors and robust numerical implementations.

   The implementation below has hardcoded weight to 1 and also has a
   tunable but hardcoded sigma normalization factor of 1.  If the user
   wants to run with different weights and normalizations, see notes
   below ("weight", "alpha" and "beta" will need to be adjusted).

   This implementation has been crafted to be completely price scale
   invariant and the result should be accurate to O(1) ulp.  If
   OVERLAP_MODEL_NEED_REF is defined, this will also declare a high
   accuracy floating point reference implementation "overlap_model_ref"
   to aid in testing / tuning block chain suitable approximations. */

#include "../util/fxp.h"
#include "../util/sar.h"

#ifdef OVERLAP_MODEL_NEED_REF
#include <math.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

#ifdef OVERLAP_MODEL_NEED_REF

/* Compute ps/pd ~ weight sqrt( 2 sigma_0 sigma_1 / (sigma_0^2 + sigma_1^2) )
                   exp( -0.5 norm^2 (mu_0-mu_1)^2 / (sigma_0^2 + sigma_1^2) )
   where weight==1 and norm==1.  If sigma_0 is not positive, it will be
   treated as zero.  Similarly for sigma_1.  Assumes inputs are tame
   such that there are no risks of nans, infs or intermediate overflows. */

static inline long double
overlap_model_ref( long double mu_0, long double sigma_0,
                   long double mu_1, long double sigma_1 ) {
  static long double const weight = 1.L;
  static long double const norm   = 1.L;
  if( !(sigma_0>0.L) ) sigma_0 = 0.L;
  if( !(sigma_1>0.L) ) sigma_1 = 0.L;
  if( sigma_0==0.L && sigma_1==0.L ) return mu_0==mu_1 ? 1.L : 0.L;
  long double kappa_sq = 1.L/(sigma_0*sigma_0 + sigma_1*sigma_1);
  long double delta    = mu_0-mu_1;
  return weight*sqrtl( (2.L*kappa_sq)*(sigma_0*sigma_1) )*expl( -((0.5L*norm*norm)*kappa_sq)*(delta*delta) );
}

#endif

static inline uint64_t
overlap_model( uint64_t mu_0, uint64_t sigma_0,
               uint64_t mu_1, uint64_t sigma_1 ) {
  uint64_t ovfl;

  /* Methodology:

     If we compute sqrt and exp terms of r separately, the sqrt term
     will end up with calculations like sqrt( 1/2^30 ) ~ 1/2^15 with a
     loss of the bits after the decimal point.  This has the effect of
     roughly halving the precision in extreme cases.  To reduce the
     impact of this and speed up the calculation, we instead absorb the
     sqrt into the exp term.  Further, since exp2 is faster and more
     accurate to compute, we rewrite r as:

       r/2^30 ~ exp2( zeta_0 - zeta_1 )
       
     where:

       zeta_0 = log2( weight sqrt( 2 sigma_0 sigma_1 / (sigma_0^2 + sigma_1^2) ) )
       zeta_1 = alpha (mu_0 - mu_1)^2 / (sigma_0^2 + sigma_1^2)
       alpha  = 0.5 log2(e) norm^2 = 0.721... norm^2

     and norm is a factor of how to the model should normalize the input
     sigma values to a "Gaussian RMS equivalent".
     
     We don't have any prior price scale restrictions for mu and sigma
     so we let:

       sigma_min = min(sigma_0,sigma_1)
       sigma_max = max(sigma_0,sigma_1)

     With this we have:

       zeta_1 = delta^2 t_sigma

     where:

       delta       = (mu_0-mu_1) / sigma_max
       t_sigma     = alpha / ( 1 + ratio_sigma^2 ) ... in [alpha/2,alpha]
       ratio_sigma = sigma_min / sigma_max         ... in [0,1] when sigma_max>0

     For zeta_0, we note:

       zeta_0 = 0.5 + log2( weight ) + 0.5 log2( ratio_sigma / ( 1 + ratio_sigma^2 ) )

     To optimize this calculation we note that the log2 term can be
     rewritten in terms of t_sigma from above:

       zeta_0 = 0.5 + log2( weight ) + 0.5 log2( ratio_sigma t_sigma / alpha )
              = beta + 0.5 log2( ratio_sigma t_sigma )

     where:

       beta = 0.5 + log2( weight ) - 0.5 log2( alpha )

     Like alpha, beta can be tuned to taste to reflect how identical
     inputs distributions should be weighted.

     This yields the method (in exact math):

        sigma_max   = max( sigma_0, sigma_1 )
        if sigma_max==0, return mu_0==mu_1 ? weight : 0 ... two delta functions were input, return weight if same and 0 if not
        sigma_min   = min( sigma_0, sigma_1 )
        delta       = (mu_0-mu_1) / sigma_max
        ratio_sigma = sigma_min / sigma_max
        t_sigma     = alpha / ( 1 + ratio_sigma^2 )
        zeta_0      = beta + 0.5 log2( ratio_sigma t_sigma )
        zeta_1      = delta^2 t_sigma
        return exp2( zeta_0 - zeta_1 )

     This is straightforward to convert to fixed points as described in
     the details below. */

  /* Compute:

       sigma_min = min(sigma_0,sigma_1)
       sigma_max = max(sigma_0,sigma_1).

     If both are zero (i.e. sigma_max==0), we have two delta functions
     and we return weight if they are the same delta function (i.e.
     mu_0==mu_1) or 0 if not.  If just one is a delta function (i.e.
     sigma_max!=0 and sigma_min==0), we return 0 to save a lot of work.  */
  
  /* FIXME: HARDCODED TO weight==1 */
  static uint64_t const weight = UINT64_C(1)<<30; /* round( 2^30 weight ) */

  int      sigma_c   = sigma_0 <= sigma_1;
  uint64_t sigma_max = sigma_c ? sigma_1 : sigma_0;
  if( !sigma_max ) return mu_0==mu_1 ? weight : UINT64_C(0);

  uint64_t sigma_min = sigma_c ? sigma_0 : sigma_1;
  if( !sigma_min ) return UINT64_C(0);

  /* At this point, sigma_max >= sigma_min > 0.  Compute:

       delta/2^30 ~ |mu_0-mu_1| / sigma_max

     with round-nearest-away style rounding.  For a fixed delta, the
     overlap model result is maximized when sigma_0==sigma_1 (maximizes
     simultaneously the sqrt factor and the exp factor).  In this limit
     we have:

       r = weight exp( -0.25 norm^2 delta^2 )

     If this is less than 2^-31, the result necessarily round to zero
     regardless of sigma_0 or sigma_1.  In equations, if:

          weight exp( -0.25 norm^2 (delta/2^30)^2 ) < 2^-31
       -> -0.25 norm^2 (delta/2^30)^2 < ln( 2^-31 / weight )
       -> delta/2^30 > 2^30 sqrt( -4 ln( 2^-31 / weight ) / norm^2 )
                     ~ 9.2709 for weight==1 and norm==1

     the result rounds to zero and do not need to compute further.  This
     implies that if:

       delta > thresh

     where

       thresh ~ ceil( 2^30 sqrt( -4 ln( 2^-31 / weight ) / norm^2 ) )

     we know the result rounds to zero and can return immediately.  This
     restricts the range of delta we need to handle which in turn
     simplifies the remaining fixed point conversions.

     We can't use fxp_div_rna_fast because the div might overflow (e.g.
     mu_max==UINT64_MAX, mu_min=0, sigma_max=2 such that delta/2^30~2^63
     -> delta~2^93 but this gets handled as part of the thresh check) */

  /* FIXME: HARDCODED TO weight==1 AND norm==1 */
  static uint64_t const thresh = UINT64_C(0x251570310); /* ~33.3 bits */

  int      mu_c   = mu_0 <= mu_1;
  uint64_t mu_min = mu_c ? mu_0 : mu_1;
  uint64_t mu_max = mu_c ? mu_1 : mu_0;
  uint64_t delta  = fxp_div_rna( mu_max-mu_min, sigma_max, &ovfl );
  if( ovfl || delta>thresh ) return UINT64_C(0);

  /* Compute:

       ratio_sigma/2^30 ~ sigma_min/sigma_max
                        = (sigma_min/2^30) / (sigma_max/2^30)

     with round-nearest-away style rounding.  We can't use
     fxp_div_rna_fast here because 2^30 sigma_min might be >= 2^64.  We
     know that the result will not overflow though as
     sigma_min<=sigma_max at this point.

     FIXME: IDEALLY WE'D FIND A REPRESENTATION OF 1/SIGMA_MAX AND USE
     THAT HERE AND ABOVE TO SAVE A SLOW FXP_DIV.  THIS IS TRICKY TO MAKE
     ROBUST GIVEN LACK OF LIMITS ON DYNAMIC RANGE OF SIGMA. */

  uint64_t ratio_sigma = fxp_div_rna( sigma_min, sigma_max, &ovfl );

  /* At this point, ratio_sigma is in [0,2^30] (it might have rounded
     to zero if the sigmas are wildly different).  Compute:

       t_sigma/2^30 ~ alpha / (1+(ratio_sigma/2^30)^2)
                    ~ in [alpha/2,alpha]

     to fixed point precision.  Interval is approximate as alpha itself
     might be rounded from its ideal value.  For typical cases,
     0.5<alpha<1 and t_sigma will fit within 30-bits.  As noted above
     though, the user might fine tune alpha to fine tune normalization
     between mu and sigma.

     Note:

       d       = (2^60 + ratio_sigma^2)
       t_sigma = floor( (alpha 2^90 + (d>>1)) / d )

     (or, in code):

       uint64_t d = (UINT64_C(1)<<60) + ratio_sigma*ratio_sigma;
       uint64_t nh,nl; uwide_inc( &nh,&nl, UINT64_C(0x2e2a8ec),UINT64_C(0xa5705fc2f0000000), d>>1 ); // ~2^90 alpha (use higher precision)
       uwide_div( &nh,&nl, nh,nl, d );
       uint64_t t_sigma = nl;

     does this calculation with precise round nearest away rounding but
     it is a lot more expensive due to the use of 128b/64b division and
     the extra precision empirically doesn't improve the overall
     accuracy to justify the extra work.

     fxp_mul_rna_fast is okay as ratio_sigma <= 2^30
     fxp_add_fast is okay 2^30 + ratio_sigma^2/2^30 <= 2^31
     fxp_div_rna_fast is okay as alpha 2^30 + (2^31>>1) < 2^64 */

  /* FIXME: HARDCODED TO norm==1 */
  static uint64_t const alpha = UINT64_C(0x2e2a8eca); /* round( 2^30 0.5 log2(e) norm^2 ) */

  uint64_t t_sigma = fxp_div_rna_fast( alpha, fxp_add_fast( UINT64_C(1)<<30, fxp_mul_rna_fast( ratio_sigma, ratio_sigma ) ) );

  /* Compute:

         e + (f/2^30) ~ 0.5 log2( ratio_sigma t_sigma )
                      ~ 0.5 ( e0 + f0/2^30 )
                      = floor(e0/2) + 0.5 (e0 is odd) + f0/2^31
                      = floor(e0/2) + (2^30 (e0 is odd) + f0) / 2^31
                      = (e0>>1) + (((e0&1)<<30) + f0) / 2^31
                      ~ (e0>>1) + ((((e0&1)<<30) + f0 + 1)>>1) / 2^30
       -> e = (e0>>1) with sign extension
          f = (((e0&1)<<30)+f0+1)>>1)

     If ratio_sigma t_sigma == 0 (or rounds to zero), the log2 term is
     going to -inf such that the result will round to zero.  (Note that
     this might be a source of precision loss when sigmas are wildly
     dissimilar.)

     fxp_mul_rna_fast is okay here is ratio_sigma and t_sigma both fit
     in O(30) bits. */

  uint64_t f0; int e0 = fxp_log2_approx( &f0, fxp_mul_rna_fast( ratio_sigma, t_sigma ) );
  if( e0<-30 ) return UINT64_C(0);
  int32_t  e = sar_int32( (int32_t)e0, 1 );                          /* |e| <= 16 */
  uint64_t f = ((((uint64_t)(e0&1)) << 30) + f0 + UINT64_C(1)) >> 1;

  /* Compute:

       zeta0/2^30 ~ beta + f/2^30   + ifelse(e<0, 0, e)
       zeta1/2^30 ~ delta^2 t_sigma + ifelse(e<0,-e, 0)

     such that:

       (zeta0-zeta1)/2^30 ~ -delta^2 t_sigma + beta + 0.5 log2( ratio_sigma t_sigma )

     The ifelse trick allows handling of signed exponents without
     needing to use a signed fixed point representation.
     
     delta^2 t_sigma isn't generally safe to compute with fast fxp
     multiplies (for the current hard coded weight==1 and norm==1,
     delta*t_sigma is safe but delta^2 isn't and delta*(delta*t_sigma)
     isn't due to potential intermediate overflow risk).  We know there
     is no risk of overflow of the result though. */

  /* FIXME: HARDCODED TO WEIGHT==1 and NORM==1 */
  static uint64_t const beta = UINT64_C(0x2f14588b); /* round( 2^30 ( 0.5 + log2(weight) - 0.5*log2(alpha) ) ), fits in 30-bits */

  uint64_t zeta0 = fxp_add_fast( beta, f );
  uint64_t zeta1 = fxp_mul_rna( fxp_mul_rna( delta, delta, &ovfl ), t_sigma, &ovfl );
  if( e<INT32_C(0) ) zeta1 += ((uint64_t)(uint32_t)(-e)) << 30;
  else               zeta0 += ((uint64_t)            e ) << 30;

  /* Compute the result:

       r/2^30 ~ exp2( (zeta0-zeta1)/2^30 )

     Note that the exponent can have different signs (e.g. when
     weight>1).  So we use the appropriate exp2 variant to avoid a
     division.
     
     fxp_sub_fast is okay because we always subtract the smaller from
     the larger. */

  uint64_t r = (zeta0>=zeta1) ?  fxp_exp2_approx( fxp_sub_fast( zeta0, zeta1 ) )
                              : fxp_rexp2_approx( fxp_sub_fast( zeta1, zeta0 ) );
  return (r<weight) ? r : weight; /* Paranoia in case roundoff pushed things over weight */
}

#ifdef __cplusplus
}
#endif

#endif /* _pyth_oracle_model_overlap_model_h_ */

