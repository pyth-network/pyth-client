#ifndef _pyth_oracle_model_gap_model_h_
#define _pyth_oracle_model_gap_model_h_

/* gap_model computes our apriori belief how comparable
   (agg_price,agg_conf) pairs measured at different points in time are
   expected to be.

   Let ps / pd be our apriori belief that aggregated values measured at
   two different block sequence numbers (separated by gap where gap is
   strictly positive) are representative of statistically similar /
   distinct price distributions (note that ps+pd=1).  Then gap_model
   computes:

     r/2^30 ~ ps / pd

   The specific model used below is:

     ps ~ exp( -gap / lambda ) 

   where lambda is the timescale (in block sequence numbers) over which
   it is believed to be sensible to consider (agg_price,agg_conf)
   price distribution parameterizations comparable.  This model reduces
   to a standard EMA when gap is always 1 but naturally generalizes to
   non-uniform sampling (e.g. chain congestion), large gaps (e.g.
   EOD-to-next-SOD) and initialization (gap->infinity).  For this model,
   lambda is known at compile time (see GAP_LAMBA) and on return r will
   be in [0,lambda 2^30].  Gap==0 should not be passed to this model but
   for specificity, the model here returns 0 if it is.

   For robustness and accuracy of the finite precision computation,
   lambda should be in 1 << lambda << 294801.  The precision of the
   calculation is impacted by the EXP2M1_FXP_ORDER (default value is
   beyond IEEE single precision accurate).

   Note that by modifying this we can correct for subtle sampling
   effects for specific chains and/or symbols (SOD, EOD, impact of
   different mechanisms for chain congestion) without needing to modify
   anything else.  We could even incorporate time invariant processes by
   tweaking the API here to take the block sequence numbers of the
   samples in question. */

#include "../util/exp2m1.h"

#ifdef __cplusplus
extern "C" {
#endif

/* GAP_MODEL_LAMBDA should an integer in [1,294802)
   (such that 720 GAP_MODEL_LAMBDA^3 < 2^64 */

#ifndef GAP_MODEL_LAMBDA
#define GAP_MODEL_LAMBDA (3000)
#endif

static inline uint64_t      /* In [0,2^30 GAP_MODEL_LAMBDA] */
gap_model( uint64_t gap ) { /* Non-zero */

  /* Let x = gap/lambda.  Then:

       r/2^30 = ps / pd = exp(-x) / ( 1 - exp(-x) )

     For x<<1, the series expansion is:

       r/2^30 ~ 1/x - 1/2 + (1/12) x - (1/720) x^3 + (1/30240) x^5 - ...

     Noting that this is a convergent series with alternating terms, the
     error in truncated this series at the x^3 term is at most the x^5
     term.  Requiring then the x^5 term to be at most 2^-31 guarantees
     the series truncation error will be at most 1/2 ulp.  This implies
     the series can be used for a practically full precision if:

       x <~ (30240 2^-31)^(1/5) ~ 0.10709...
       -> gap < 0.10709... lambda

     The truncated series in terms of gap can be written as:

       r/2^30 ~ lambda/gap - 1/2 + gap/(12 lambda) - gap^3/(720 lambda^3)
              = (lambda + gap/2 + gap^2 ( 1/(12 lambda) - gap^2 (1/(720 lambda^3)) )) / gap

     Convering the division to an integer division with grade school
     rounding:

       r ~ floor( ( 2^(30+s) lambda + gap (2^(29+s)-2^(s-1))
                  + gap^2 ( 2^(30+s)/(12 lambda) - gap^2 2^(30+s) / (720 lambda^3) ) ) / (2^s gap) )

     where s is a positive integer picked to maximize precision while
     not causing intermediate overflow.  We require 720 lambda^3 < 2^64
     -> lambda <= lambda_max = 294801 (such that the coefficients above
     are straightforward to compute at compile time).
     
     Given gap <= 0.10709 lambda_max, gap^2 can be computed exactly
     without overflow.  Adding an intermediate scale factor to improve
     the accuracy of the correction coefficients then yields:

       r ~ floor( cp + gap c0 + floor((gap^2 (c1 - gap^2 c3) + 2^(t-1)) / 2^t) ) / (2^s gap) )

     where:

       cp = 2^(30+s) lambda
       c0 = 2^(29+s)-2^(s-1)
       c1 = 2^(30+s+t)/(12 lambda)
       c3 = 2^(30+s+t)/(720 lambda^3)

     To prevent overflow in the c1 and c3 corrections, it is sufficient that:

       s+t < log(12 2^34 / lambda_max ) ~ 22.7

     So we use s+t==22.  Maximizing precision in the correction
     calculation then is achieved with s==1 and t==21.  (The pole and
     constant calculations are exact so long as s is positive.)  This
     yields the small gap approximation:

       r ~ floor( (cp + gap c0 + (gap^2 ( c1 - gap^2 c3 ) + 2^20)>>21) / 2 gap) )

       cp = 2^31 lambda
       c0 = 2^30 - 1
       c1 = floor( (2^52 + 6 lambda)/(12 lambda) )
       c3 = floor( (2^52 + 360 lambda^3)/(720 lambda^3) )

     This is ~30-bits accurate when gap < 0.10709... lambda.

     For x>>1, multiplying by 1 = exp(x) / exp(x) yields:

       r/2^30 = 1    / ( exp(x)       - 1 )
              = 1    / ( exp2(x log_2 e) - 1 )
              = 1    / ( exp2( gap ((2^30 log_2 e)/lambda) ) - 1 )
              ~ 1    / exp2m1( (gap c + 2^(s-1)) >> s )
              = 2^30 / exp2m1_fxp( (gap c + 2^(s-1)) >> s )

     where c = round( (2^30+s) log_2 e / lambda ) and s is picked to
     maximize precision of computing gap / lambda without causing
     overflow.  Further evaluating:

       c ~ ((2^(31+s)) log_2 e + lambda) / (lambda<<1)
       d = exp2m1_fxp( (gap*c + 2^(s-1)) >> s )
       r ~ (2^61 + d) / (d<<1)

     For the choice of s, we note that if:

       exp(-x) / (1-exp(-x)) <= 2^-31
       (1+2^-31) exp(-x) <= 2^-31
       exp(-x) <= 2^-31/(1+2^-31)
       x >= -ln (2^31/(1+2^-31)) ~ 21.488...

     The result will round to zero.  Thus, we only care about cases
     where:

       gap < thresh 

     and:

       thresh ~ 21.5 lambda

     We thus require:

       c (thresh-1) + 2^(s-1) < 2^64
       (2^(30+s)) log_2 e 21.5 < ~2^64
       2^s <~ 2^34 / (21.5 log_2 e)
       s <~ 34 - log_2 (21.5 log_2 e ) ~ 29.045...

     So we use s == 29 when computing compile time constant c.

     (Note: while it looks like we could just use the x>>1 approximation
     everywhere, it isn't so accurate for gap ~ 1 as the error in the
     c*gap is amplified exponentially into the output.) */

  /* If we need the behavior that this never returns 0 (i.e. we are
     never 100 percent confident that samples separated by a large gap
     cannot be compared), tweak big_thresh for the condition

       exp(-x) / (1-exp(-x)) <= 3 2^-31

     and return 1 here. */

  static uint64_t const big_thresh = (UINT64_C(43)*(uint64_t)GAP_MODEL_LAMBDA) >> 1;
  if( !gap || gap>=big_thresh ) return UINT64_C(0);

  /* round( 0.10709... lambda ) <= 3191 */
//static uint64_t const small_thresh = (UINT64_C(0x1b69f363043ef)*(uint64_t)GAP_MODEL_LAMBDA + (UINT64_C(1)<<51)) >> 52;

  /* hand tuned value minimizes worst case ulp error in the
     EXP2M1_ORDER=7 case */
  static uint64_t const small_thresh = (UINT64_C(0x2f513cc1e098e)*(uint64_t)GAP_MODEL_LAMBDA + (UINT64_C(1)<<51)) >> 52;

  if( gap<=small_thresh ) {

    /* 2^31 lambda, exact */
    static uint64_t const cp = ((uint64_t)GAP_MODEL_LAMBDA)<<31;

    /* 2^30 - 1, exact */
    static uint64_t const c0 = (UINT64_C(1)<<30) - UINT64_C(1);

    /* round( 2^52 / (12 lambda) ) */
    static uint64_t const c1 = ((UINT64_C(1)<<52) + UINT64_C(6)*(uint64_t)GAP_MODEL_LAMBDA)
                             / (UINT64_C(12)*(uint64_t)GAP_MODEL_LAMBDA);

    /* round( 2^52 / (720 lambda^3) ) */
    static uint64_t const c3 = ((UINT64_C(1)<<52) + UINT64_C(360)*((uint64_t)GAP_MODEL_LAMBDA)*((uint64_t)GAP_MODEL_LAMBDA)*((uint64_t)GAP_MODEL_LAMBDA))
                             / (UINT64_C(720)*((uint64_t)GAP_MODEL_LAMBDA)*((uint64_t)GAP_MODEL_LAMBDA)*((uint64_t)GAP_MODEL_LAMBDA));

    uint64_t gap_sq = gap*gap; /* exact, at most 3191^2 (no overflow) */
    return (cp - c0*gap + (( gap_sq*(c1 - gap_sq*c3) + (UINT64_C(1)<<20))>>21)) / (gap<<1);
  }

  /* round( 2^59 log_2 / lambda ) */
  static uint64_t const c = ((UINT64_C(0x171547652b82fe18)) + (uint64_t)GAP_MODEL_LAMBDA) / (((uint64_t)GAP_MODEL_LAMBDA)<<1);

  uint64_t d = exp2m1_fxp( (gap*c + (UINT64_C(1)<<28)) >> 29 );
  return ((UINT64_C(1)<<61) + d) / (d<<1);
}

#ifdef __cplusplus
}
#endif

#endif /* _pyth_oracle_model_gap_model_h_ */
