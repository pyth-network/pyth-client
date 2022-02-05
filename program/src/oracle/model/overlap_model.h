#ifndef _pyth_oracle_model_overlap_model_h_
#define _pyth_oracle_model_overlap_model_h_

/* overlap_model computes our prior belief whether two
   (agg_price,agg_conf) estimates parameterize the same underlying price
   distribution.  Specifically, let ps / pd be our apriori belief that
   two different price distribution estimates came from the same /
   different underlying price distribution.  Then overlap_model
   computes:

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
   weight is the ps / pd prior to use for identical estimates.  As such
   the output is in [0,weight 2^30].  This model is heuristic but well
   motivated theoretically with good limiting behaviors and robust
   numerical implementations.

   weight can be specified by defining the compile time constant
   OVERLAP_MODEL_WEIGHT to an expression that should evaluate at compile
   time to round( 2^30 weight ).  If not specified, the model implements
   the case weight=1 / ps=pd=0.5.  That is, identical estimates are
   considered to be a coin toss between sampling the same distribution
   or sampling different distributions that are close enough to be
   statistically indistinguishable.

   This implementation has been crafted to be completely price scale
   invariant and the result should be accurate to roughly that of
   REXP2_FXP_ORDER (in the most precise case, nearly accurate to full
   precision) when sigma0 and sigma1 are comparable in magnitude.  When
   sigma0 and sigma1 are really dissimilar in magnitude (i.e.
   sigma_min/sigma_max<<1), the limitations of the fixed point
   representation limits accuracy at most ~3e-5.  The default
   REXP2_FXP_ORDER is accurate to better than IEEE single precision when
   sigma_0 and sigma_1 have comparable magnitudes. */

#include "../util/uwide.h"
#include "../util/sqrt.h"
#include "../util/rexp2.h"
#include "../util/log2.h"

#ifdef __cplusplus
extern "C" {
#endif

/* OVERLAP_MODEL_WEIGHT should be a uint64_t up to ~2^34 */

#ifndef OVERLAP_MODEL_WEIGHT
#define OVERLAP_MODEL_WEIGHT (UINT64_C(1)<<30)
#endif

static inline uint64_t                              /* In [0,OVERLAP_MODEL_WEIGHT] */
overlap_model( uint64_t mu_0, uint64_t sigma_0,
               uint64_t mu_1, uint64_t sigma_1 ) {
  uint64_t tmp_h,tmp_l;

  /* Order sigma (this will enable use to do a price scale invariant
     method) and handle the case of two delta functions (shouldn't
     happen but want to give well defined reasonable behavior in all
     circumstances). */

  int      sigma_c   = sigma_0 < sigma_1;
  uint64_t sigma_min = sigma_c ? sigma_0 : sigma_1;
  uint64_t sigma_max = sigma_c ? sigma_1 : sigma_0;
  if( !sigma_max ) return mu_0==mu_1 ? OVERLAP_MODEL_WEIGHT : UINT64_C(0);

  /* Compute ratio_sigma/2^30 ~ round( sigma_min / sigma_max ) via:

       ratio_sigma ~ floor( (2^30 sigma_min + (sigma_max>>1)) / sigma_max )

     Notes:
     - ratio_sigma is in [0,2^30] and price scale invariant.
     - price scale invariance requires us construct the numerator in
       128-bits
     - the denominator at this point is not zero so the division is safe
     - rounding in this calc would be very slightly better via:

         floor( (2^31 sigma_min + sigma_max) / (2 sigma_max) )

       but this either requires imposing a price scale limitation (i.e.
       sigma_max < 2^63) or using a very expensive 128 bit / 128 bit
       divider for something that practically already is overkill. */

  uwide_sl(  &tmp_h,&tmp_l, UINT64_C(0),sigma_min, 30 );
  uwide_add( &tmp_h,&tmp_l, tmp_h,tmp_l, UINT64_C(0),UINT64_C(0), sigma_max>>1 );
  uwide_div( &tmp_h,&tmp_l, tmp_h,tmp_l, sigma_max );
  uint64_t ratio_sigma = tmp_l; /* In [0,2^30], note that tmp_h==0 at this point */

  /* Rewriting the model in terms of ratio_sigma, we have:

       r/2^30 ~ sqrt( ratio_sigma/2^30 )
                (kappa/2^30)
                exp( -(0.5 (kappa/2^30) (mu_max-mu_min) / sigma_max)^2 )

     where:

       kappa/2^30 ~ sqrt( 2 / ( 1 + (ratio_sigma/2^30)^2 ) )

     Compute kappa via:

       tmp = 2^60 + ratio_sigma^2
       kappa ~ sqrt_uint64( floor( (2^122 + tmp) / (2 tmp) ) )

     Note that the denominator will not overflow and the result will be
     in [2^30,2^30.5).  (This uses floor rounding for the sqrt here but
     too expensive to do it otherwise.  Already probably massive
     overkill.)  We can short circuit the case where ratio_sigma==0
     given the above. */

  if( !ratio_sigma ) return UINT64_C(0);
  tmp_l = (UINT64_C(1)<<60) + ratio_sigma*ratio_sigma; /* In [2^60,2^61] */
  uwide_div( &tmp_h,&tmp_l, UINT64_C(1)<<(122-64),tmp_l, tmp_l<<1 );
  uint64_t kappa = sqrt_uint64( tmp_l );               /* In [2^30,2^30.5) */

  /* The exponential factor can be written as:

       omega/2^30 ~ exp2( -xsq/2^30 )

     where:
     
       xsq/2^30 ~ (x/2^30)^2
       
     and:

       x/2^30 ~ 0.5 sqrt(log2(e)) (kappa/2^30) (mu_max-mu_min) / sigma_max
     
     We note that if exp2( -xsq/2^30 ) <= 2^-31, the result rounds to
     zero.  This implies we can shortcut if xsq >= 31 2^30 or:

       (x/2^30)^2 >= 31 2^30 -> x >= ceil( sqrt(31)*2^45 ) ~ 0x22b...

     We compute x via:

       x ~ floor( (floor( (c kappa + 2^(s-1)) / 2^s ) (mu_max - mu_min) + (sigma_max>>1) ) / sigma_max )

     and:

       c = round( 0.5 sqrt(log2(e)) 2^s ) ~ 0x266...

     and s==34 (largest s such that c kappa + 2^(s-1) cannot overflow.
     Similar sigma_max division rounding considerations as above apply. */

  int      mu_c   = mu_0<mu_1;
  uint64_t mu_min = mu_c ? mu_0 : mu_1;
  uint64_t mu_max = mu_c ? mu_1 : mu_0;

  uwide_mul( &tmp_h,&tmp_l, ((UINT64_C(0x266f98430)*kappa) + (UINT64_C(1)<<33)) >> 34, mu_max-mu_min );
  uwide_add( &tmp_h,&tmp_l, tmp_h,tmp_l, UINT64_C(0),UINT64_C(0), sigma_max>>1 );
  uwide_div( &tmp_h,&tmp_l, tmp_h,tmp_l, sigma_max );
  if( tmp_h || tmp_l>=UINT64_C(0x22b202b460f) ) return UINT64_C(0);
  uwide_mul( &tmp_h,&tmp_l, tmp_l, tmp_l );
  uwide_add( &tmp_h,&tmp_l, tmp_h,tmp_l, UINT64_C(0),UINT64_C(0), UINT64_C(1)<<29 );
  uwide_sr( &tmp_h,&tmp_l, tmp_h,tmp_l, 30 );
  uint64_t omega = rexp2_fxp( tmp_l ); /* In [1,2^30] */

  /* Combine the above to get the result:

       r/2^30 ~ sqrt( ratio_sigma/2^30 ) (kappa/2^30) (exp_xsq/2^30) (weight/2^30)

     via:

       r ~ floor((floor((floor((sqrt(2^30 ratio_sigma)kappa + 2^29)/2^30)exp_xsq + 2^29)/2^30)weight + 2^29)/2^30)

     As ratio_sigma is in [0,2^30], kappa is in [2^30,2^30.5) and
     exp_xsq is in [0,2^30], none of these calcs will overflow. */

  static uint64_t const weight = OVERLAP_MODEL_WEIGHT;

  uint64_t r = sqrt_uint64( ratio_sigma << 30 );
  r = ((r*kappa ) + (UINT64_C(1)<<29)) >> 30;
  r = ((r*omega ) + (UINT64_C(1)<<29)) >> 30;
  r = ((r*weight) + (UINT64_C(1)<<29)) >> 30;
  if( r>weight ) r = weight; /* Paranoia */
  return r;
}

#ifdef __cplusplus
}
#endif

#endif /* _pyth_oracle_model_overlap_model_h_ */

