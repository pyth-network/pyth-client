#define REXP2_FXP_ORDER  7
#define EXP2M1_FXP_ORDER 7

#include "twap_model.h"

#include "gap_model.h"
#include "overlap_model.h"
#include "weight_model.h"
#include "leaky_integrator.h"
#include "../util/uwide.h"

twap_model_t *
twap_model_update( twap_model_t * tm,
                   uint64_t       now,
                   uint64_t       agg_price,
                   uint64_t       agg_conf ) {

  /* If this is the first observation, reset */

  if( !tm->valid ) goto reset;

  /* If sequence numbers didn't advance since last observation, reset
     (this logic also correctly handles sequence number wrapping). */

  int64_t gap = (int64_t)(now-tm->ts);
  if( gap<=INT64_C(0) ) goto reset;

  /* Compute weight/2^30 ~ our Bayesian probability that this
     (agg_price,agg_conf) estimate parameterizes a price distribution
     similiar to the most recent observed price distribution (event
     SIM). */

  uint64_t weight = weight_model( gap_model( (uint64_t)gap ), overlap_model( tm->twap,tm->twac, agg_price,agg_conf ) );

  /* The number of samples if  SIM is D+1 and prob  SIM =   weight/2^30
     The number of samples if ~SIM is 1   and prob ~SIM = 1-weight/2^30
     Expected number of recent samples available to average over then:
        (D+1)*weight + 1*(1-weight) == D*weight + 1
     Thus we can apply a leaky integrator recursion for D and similarly
     for the twap_N and twac_N.  While these should never have any
     overflow or what not given a GAP_MODEL_LAMBDA and
     OVERLAP_MODEL_WEIGHT, we reset if we have even the slightest hint
     of an issue to fail safe. */

  uint64_t tmp_h,tmp_l;

  if( leaky_integrator( &tm->twap_Nh,&tm->twap_Nl, tm->twap_Nh,tm->twap_Nl, weight, agg_price   )          ) goto reset;
  if( leaky_integrator( &tm->twac_Nh,&tm->twac_Nl, tm->twac_Nh,tm->twac_Nl, weight, agg_conf    )          ) goto reset;
  if( leaky_integrator( &tmp_h,      &tm->D,       UINT64_C(0),tm->D,       weight, UINT64_C(1) ) || tmp_h ) goto reset;

  /* Compute twap ~ twap_N/D via:
       twap = floor( (twap_N+(D/2)) / D )
     and similarly for twac. */

  tm->valid = INT8_C(1);
  tm->ts    = now;
  if( uwide_add( &tmp_h,&tmp_l, tm->twap_Nh,tm->twap_Nl, UINT64_C(0),UINT64_C(0), tm->D>>1 ) ) goto reset;
  if( uwide_div( &tmp_h,&tm->twap, tmp_h,tmp_l, tm->D ) || tmp_h                             ) goto reset;
  if( uwide_add( &tmp_h,&tmp_l, tm->twac_Nh,tm->twac_Nl, UINT64_C(0),UINT64_C(0), tm->D>>1 ) ) goto reset;
  if( uwide_div( &tmp_h,&tm->twac, tmp_h,tmp_l, tm->D ) || tmp_h                             ) goto reset;
  return tm;

reset:
  tm->valid = INT8_C(1);
  tm->ts    = now;
  tm->twap  = agg_price;
  tm->twac  = agg_conf;
  uwide_sl( &tm->twap_Nh,&tm->twap_Nl, UINT64_C(0),agg_price, 30 );
  uwide_sl( &tm->twac_Nh,&tm->twac_Nl, UINT64_C(0),agg_conf,  30 );
  tm->D     = UINT64_C(1)<<30;
  return tm;
}

