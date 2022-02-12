#ifndef _pyth_oracle_upd_aggregrate_h_
#define _pyth_oracle_upd_aggregrate_h_

#include <limits.h> /* For INT64_MAX */

#include "oracle.h"
#include "model/price_model.h"
#include "model/twap_model.h"

/* FIXME: TEMP HACK TO DEAL WITH LINKING */
#include "model/price_model.c"
#include "model/twap_model.c"

#ifdef __cplusplus
extern "C" {
#endif

// update aggregate price

static inline void
upd_aggregate( pc_price_t *ptr,
               uint64_t    now ) {

  // only re-compute aggregate in newer slots
  // this logic correctly handles sequence number wrapping

  int64_t agg_diff = (int64_t)(now - ptr->last_slot_);
  if( agg_diff<INT64_C(0) ) return;

  // copy previous price

  ptr->prev_slot_  = ptr->valid_slot_;
  ptr->prev_price_ = ptr->agg_.price_;
  ptr->prev_conf_  = ptr->agg_.conf_;

  // update aggregate details ready for next slot

  ptr->valid_slot_    = ptr->agg_.pub_slot_; // valid slot-time of agg. price
  ptr->agg_.pub_slot_ = now;                 // publish slot-time of agg. price

  // identify valid quotes (those that have a trading status, have a
  // recent quote (i.e. from a block seq number
  // [now-PC_MAX_SEND_LATENCY,now]) and do not have obviously bad values
  // for price and conf for that quote (i.e. 0<conf<price and
  // conf<=INT64_MAX-price) and if there are enough of them, apply the
  // price aggegration model to these quotes

  uint32_t numv = UINT32_C(0);
  int64_t  agg_price;
  int64_t  agg_conf;
  {
    uint32_t  nprcs = UINT32_C(0);
    int64_t * prcs  = (int64_t *)PC_HEAP_START; // Uses at most ~1.5KiB for current PC_COMP_SIZE in heap area (FIXME: USE MALLOC?)
    for( uint32_t i=UINT32_C(0); i<ptr->num_; i++ ) {
      pc_price_comp_t *iptr = &ptr->comp_[i];
      // copy contributing price to aggregate snapshot
      iptr->agg_ = iptr->latest_;
      // add quote values to prcs if valid
      int64_t slot_diff = (int64_t)(now - iptr->agg_.pub_slot_);
      int64_t price     = iptr->agg_.price_;
      int64_t conf      = ( int64_t )( iptr->agg_.conf_ );
      if( iptr->agg_.status_ == PC_STATUS_TRADING &&
          INT64_C(0)<=slot_diff && slot_diff<=PC_MAX_SEND_LATENCY &&
          INT64_C(0)<conf && conf<price &&
          conf <= (INT64_MAX-price) ) { // No overflow for INT64_MAX-price as price>0
        numv++;
        prcs[ nprcs++ ] = price - conf; // No overflow as 0 < conf < price
        prcs[ nprcs++ ] = price;
        prcs[ nprcs++ ] = price + conf; // No overflow as 0 < conf <= INT64_MAX-price
      }
    }

    // too few valid quotes
    if( !numv || numv<ptr->min_pub_ ) {
      // FIXME: SHOULD NUM_QT_ AND/OR LAST_SLOT_ BE UPDATED?  IN EARLIER
      // CODE, NUM_QT_ WAS BUT LAST SLOT WASN'T IN CASES LIKE THIS.  IT
      // SEEMS LIKE IT WAS WRONG TO UPDATE NUM_QT_ THOUGH AS IT SEEMS TO
      // CORRESPOND THE POINT IN TIME SPECIFIED BY LAST_SLOT_.
      ptr->agg_.status_ = PC_STATUS_UNKNOWN;
      return;
    }

    // evaluate price_model to get the p25/p50/p75 prices
    // note: numv>0 and nprcs = 3*numv at this point
    int64_t agg_p25;
    int64_t agg_p75;
    price_model_core( (uint64_t)nprcs, prcs, &agg_p25, &agg_price, &agg_p75, prcs+nprcs );

    // get the left and right confidences
    // note that as valid quotes have positive prices currently and
    // agg_p25, agg_price, agg_p75 are ordered, this calculation can't
    // overflow / underflow
    int64_t agg_conf_left  = agg_price - agg_p25;
    int64_t agg_conf_right = agg_p75 - agg_price;

    // use the larger of the left and right confidences
    agg_conf = agg_conf_right > agg_conf_left ? agg_conf_right : agg_conf_left;

    // if the aggregated values don't look sane, we abort
    // this is paranoia given the current pricing model and
    // above quoter sanity checks
    if( !(INT64_C(0)<agg_conf && agg_conf<agg_price && agg_conf<=(INT64_MAX-agg_price)) ) {
      // FIXME: SEE NOTE ABOVE ABOUT NUM_QT_ AND LAST_SLOT_
      ptr->agg_.status_ = PC_STATUS_UNKNOWN;
      return;
    }
  }

  // we have an valid (now,agg_price,agg_conf) observation
  // update the twap_model

  // FIXME: HIDEOUS TEMPORARY HACK TO PACK THE EXISTING PC_PRICE_T DATA
  // LAYOUT INTO A TWAP_MODEL_T (PC_PRICE_T SHOULD JUST HAVE THIS AS A
  // COMPONENT AND MAYBE ELIMINATE LAST_SLOT_ IN FAVOR OF TM->TS).
  twap_model_t tm[1];
  tm->valid   = ptr->drv2_;                   // FIXME: MAKE SURE DRV2_ IS INITIALIZED TO ZERO ON ACCOUNT CREATION
  tm->ts      = ptr->last_slot_;
  tm->twap    = (uint64_t)ptr->twap_.val_;
  tm->twac    = (uint64_t)ptr->twac_.val_;
  tm->twap_Nh = (uint64_t)ptr->twap_.numer_; tm->twap_Nl = (uint64_t)ptr->twap_.denom_; // FIXME: UGLY REINTERP
  tm->twac_Nh = (uint64_t)ptr->twac_.numer_; tm->twac_Nl = (uint64_t)ptr->twac_.denom_; // FIXME: UGLY REINTERP
  tm->D       = (uint64_t)ptr->drv1_;

  twap_model_update( tm, now, (uint64_t)agg_price, (uint64_t)agg_conf );

  // FIXME: HIDEOUS TEMPORARY HACK TO UNPACK.  SEE NOTE ABOVE.  IF
  // ANYTHING EVEN WENT THE SLIGHTEST WEIRD, WE ABORT.
  if( !(INT64_C(0)<tm->twac && tm->twac<tm->twap && tm->twac<=((uint64_t)INT64_MAX-tm->twap)) ) {
    // FIXME: SEE NOTE ABOVE ABOUT NUM_QT_ AND LAST_SLOT_
    ptr->agg_.status_ = PC_STATUS_UNKNOWN;
    return;
  }
  ptr->drv2_        = tm->valid;
  ptr->twap_.val_   = (int64_t)tm->twap;
  ptr->twac_.val_   = (int64_t)tm->twac;
  ptr->twap_.numer_ = (int64_t)tm->twap_Nh; ptr->twap_.denom_ = (int64_t)tm->twap_Nl;
  ptr->twac_.numer_ = (int64_t)tm->twac_Nh; ptr->twac_.denom_ = (int64_t)tm->twac_Nl;
  ptr->drv1_        = (int64_t)tm->D;

  // all good
  ptr->num_qt_      = numv;
  ptr->last_slot_   = now;
  ptr->agg_.price_  = agg_price;
  ptr->agg_.conf_   = (uint64_t)agg_conf;
  ptr->agg_.status_ = PC_STATUS_TRADING;
}

#ifdef __cplusplus
}
#endif

#endif /* _pyth_oracle_upd_aggregrate_h_ */
