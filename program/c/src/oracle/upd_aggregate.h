#pragma once

#include "oracle.h"
#include "model/price_model.h"
#include "model/price_model.c" /* FIXME: HACK TO DEAL WITH DOCKER LINKAGE ISSUES */
#include "pd.h"


#ifdef __cplusplus
extern "C" {
#endif

typedef struct pc_qset
{
  pd_t      iprice_[PC_COMP_SIZE];
  pd_t      uprice_[PC_COMP_SIZE];
  pd_t      lprice_[PC_COMP_SIZE];
  pd_t      weight_[PC_COMP_SIZE];
  int64_t   decay_[1+PC_MAX_SEND_LATENCY];
  int64_t   fact_[PC_FACTOR_SIZE];
} pc_qset_t;

// initialize quote-set temporary data in heap area
static pc_qset_t *qset_new()
{
  // allocate off heap
  pc_qset_t *qs = (pc_qset_t*)PC_HEAP_START;

  // sqrt of numbers 1 to 25 for decaying conf. interval based on slot delay
  qs->decay_[0]  = 1000000000L;
  qs->decay_[1]  = 1000000000L;
  qs->decay_[2]  = 1414213562L;
  qs->decay_[3]  = 1732050807L;
  qs->decay_[4]  = 2000000000L;
  qs->decay_[5]  = 2236067977L;
  qs->decay_[6]  = 2449489742L;
  qs->decay_[7]  = 2645751311L;
  qs->decay_[8]  = 2828427124L;
  qs->decay_[9]  = 3000000000L;
  qs->decay_[10] = 3162277660L;
  qs->decay_[11] = 3316624790L;
  qs->decay_[12] = 3464101615L;
  qs->decay_[13] = 3605551275L;
  qs->decay_[14] = 3741657386L;
  qs->decay_[15] = 3872983346L;
  qs->decay_[16] = 4000000000L;
  qs->decay_[17] = 4123105625L;
  qs->decay_[18] = 4242640687L;
  qs->decay_[19] = 4358898943L;
  qs->decay_[20] = 4472135954L;
  qs->decay_[21] = 4582575694L;
  qs->decay_[22] = 4690415759L;
  qs->decay_[23] = 4795831523L;
  qs->decay_[24] = 4898979485L;
  qs->decay_[25] = 5000000000L;

  // powers of 10 for use in decimal arithmetic scaling
  qs->fact_[0]   = 1L;
  qs->fact_[1]   = 10L;
  qs->fact_[2]   = 100L;
  qs->fact_[3]   = 1000L;
  qs->fact_[4]   = 10000L;
  qs->fact_[5]   = 100000L;
  qs->fact_[6]   = 1000000L;
  qs->fact_[7]   = 10000000L;
  qs->fact_[8]   = 100000000L;
  qs->fact_[9]   = 1000000000L;
  qs->fact_[10]  = 10000000000L;
  qs->fact_[11]  = 100000000000L;
  qs->fact_[12]  = 1000000000000L;
  qs->fact_[13]  = 10000000000000L;
  qs->fact_[14]  = 100000000000000L;
  qs->fact_[15]  = 1000000000000000L;
  qs->fact_[16]  = 10000000000000000L;
  qs->fact_[17]  = 100000000000000000L;

  return qs;
}

static void upd_ema(
    pc_ema_t *ptr, pd_t *val, pd_t *conf, int64_t nslot, pc_qset_t *qs, int32_t expo
    )
{
  pd_t numer[1], denom[1], cwgt[1], wval[1], decay[1], diff[1], one[1];
  pd_new( one, 100000000L, -8 );
  if ( conf->v_ ) {
    pd_div( cwgt, one, conf );
  } else {
    pd_set( cwgt, one );
  }
  if ( nslot > PD_EMA_MAX_DIFF ) {
    // initial condition
    pd_mul( numer, val, cwgt );
    pd_set( denom, cwgt );
  } else {
    // compute decay factor
    pd_new( diff, nslot, 0 );
    pd_new( decay, PD_EMA_DECAY, PD_EMA_EXPO );
    pd_mul( decay, decay, diff );
    pd_add( decay, decay, one, qs->fact_ );

    // compute numer/denom and new value from decay factor
    pd_load( numer, ptr->numer_ );
    pd_load( denom, ptr->denom_ );
    if ( numer->v_ < 0 || denom->v_ < 0 ) {
      // temporary reset twap on negative value
      pd_set( numer, val );
      pd_set( denom, one );
    }
    else {
      pd_mul( numer, numer, decay );
      pd_mul( wval, val, cwgt );
      pd_add( numer, numer, wval, qs->fact_ );
      pd_mul( denom, denom, decay );
      pd_add( denom, denom, cwgt, qs->fact_ );
      pd_div( val, numer, denom );
    }
  }

  // adjust and store results
  pd_adjust( val, expo, qs->fact_ );
  ptr->val_   = val->v_;
  int64_t numer1, denom1;
  if ( pd_store( &numer1, numer ) && pd_store( &denom1, denom ) ) {
    ptr->numer_ = numer1;
    ptr->denom_ = denom1;
  }
}

static inline void upd_twap(
    pc_price_t *ptr, int64_t nslots )
{
  pc_qset_t *qs = qset_new( );

  pd_t px[1], conf[1];
  pd_new_scale( px, ptr->agg_.price_, ptr->expo_ );
  pd_new_scale( conf, ( int64_t )( ptr->agg_.conf_ ), ptr->expo_ );
  upd_ema( &ptr->twap_, px, conf, nslots, qs, ptr->expo_ );
  upd_ema( &ptr->twac_, conf, conf, nslots, qs, ptr->expo_ );
}

// update aggregate price
static inline bool upd_aggregate( pc_price_t *ptr, uint64_t slot, int64_t timestamp )
{
  // only re-compute aggregate in next slot
  if ( slot <= ptr->agg_.pub_slot_ ) {
    return false;
  }

  // get number of slots from last published valid price
  int64_t agg_diff = ( int64_t )slot - ( int64_t )( ptr->last_slot_ );

  // Update the value of the previous price, if it had TRADING status.
  if ( ptr->agg_.status_ == PC_STATUS_TRADING ) {
    ptr->prev_slot_      = ptr->agg_.pub_slot_;
    ptr->prev_price_     = ptr->agg_.price_;
    ptr->prev_conf_      = ptr->agg_.conf_;
    ptr->prev_timestamp_ = ptr->timestamp_;
  }

  // update aggregate details ready for next slot
  ptr->valid_slot_ = ptr->agg_.pub_slot_;// valid slot-time of agg. price
  ptr->agg_.pub_slot_ = slot;            // publish slot-time of agg. price
  ptr->timestamp_ = timestamp;

  // identify valid quotes
  // compute the aggregate prices and ranges
  int64_t  agg_price;
  int64_t  agg_conf;
  {
    uint32_t numv  = 0;
    uint32_t nprcs = (uint32_t)0;
    int64_t  prcs[ PC_COMP_SIZE * 3 ]; // ~0.75KiB for current PC_COMP_SIZE (FIXME: DOUBLE CHECK THIS FITS INTO STACK FRAME LIMIT)
    for ( uint32_t i = 0; i != ptr->num_; ++i ) {
      pc_price_comp_t *iptr = &ptr->comp_[i];
      // copy contributing price to aggregate snapshot
      iptr->agg_ = iptr->latest_;
      // add quote to sorted permutation array if it is valid
      int64_t slot_diff = ( int64_t )slot - ( int64_t )( iptr->agg_.pub_slot_ );
      int64_t price     = iptr->agg_.price_;
      int64_t conf      = ( int64_t )( iptr->agg_.conf_ );
      if ( iptr->agg_.status_ == PC_STATUS_TRADING &&
           // No overflow for INT64_MIN+conf or INT64_MAX-conf as 0 < conf < INT64_MAX
           // These checks ensure that price - conf and price + conf do not overflow.
           (int64_t)0 < conf && (INT64_MIN + conf) <= price && price <= (INT64_MAX-conf) &&
           slot_diff >= 0 && slot_diff <= PC_MAX_SEND_LATENCY ) {
        numv += 1;
        prcs[ nprcs++ ] = price - conf;
        prcs[ nprcs++ ] = price;
        prcs[ nprcs++ ] = price + conf;
      }
    }

    // too few valid quotes
    ptr->num_qt_ = numv;
    if ( numv == 0 || numv < ptr->min_pub_ ) {
      ptr->agg_.status_ = PC_STATUS_UNKNOWN;
      return false;
    }

    // evaluate the model to get the p25/p50/p75 prices
    // note: numv>0 and nprcs = 3*numv at this point
    int64_t agg_p25;
    int64_t agg_p75;
    int64_t scratch[ PC_COMP_SIZE * 3 ]; // ~0.75KiB for current PC_COMP_SIZE (FIXME: DOUBLE CHECK THIS FITS INTO STACK FRAME LIMIT)
    price_model_core( (uint64_t)nprcs, prcs, &agg_p25, &agg_price, &agg_p75, scratch );

    // get the left and right confidences
    // note that as valid quotes have positive prices currently and
    // agg_p25, agg_price, agg_p75 are ordered, this calculation can't
    // overflow / underflow
    int64_t agg_conf_left  = agg_price - agg_p25;
    int64_t agg_conf_right = agg_p75 - agg_price;

    // use the larger of the left and right confidences
    agg_conf = agg_conf_right > agg_conf_left ? agg_conf_right : agg_conf_left;

    // if the confidences end up at zero, we abort
    // this is paranoia as it is currently not possible when nprcs>2 and
    // positive confidences given the current pricing model
    if( agg_conf <= (int64_t)0 ) {
      ptr->agg_.status_ = PC_STATUS_UNKNOWN;
      return false;
    }
  }

  // update status and publish slot of last trading status price
  ptr->agg_.status_ = PC_STATUS_TRADING;
  ptr->last_slot_   = slot;
  ptr->agg_.price_  = agg_price;
  ptr->agg_.conf_   = (uint64_t)agg_conf;

  upd_twap( ptr, agg_diff );
  return true;
}

#ifdef __cplusplus
}
#endif
