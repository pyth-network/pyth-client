#pragma once

#include "oracle.h"
#include "pd.h"
#include "sort.h"

#include <limits.h> // NOLINT(modernize-deprecated-headers)

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
  int32_t   expo_;
} pc_qset_t;

// initialize quote-set temporary data in heap area
static pc_qset_t *qset_new( int expo )
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

  qs->expo_ = expo;

  return qs;
}

static void upd_ema(
    pc_ema_t *ptr, pd_t *val, pd_t *conf, int64_t nslot, pc_qset_t *qs
    , pc_price_t *prc_ptr)
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
    if ( prc_ptr->drv1_ ) {
      pd_load( numer, ptr->numer_ );
      pd_load( denom, ptr->denom_ );
    }
    else {
      // temporary upgrade code
      pd_new_scale( numer, ptr->numer_, PD_EMA_EXPO );
      pd_new_scale( denom, ptr->denom_, PD_EMA_EXPO );
    }
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
  pd_adjust( val, qs->expo_, qs->fact_ );
  ptr->val_   = val->v_;
  int64_t numer1, denom1;
  if ( pd_store( &numer1, numer ) && pd_store( &denom1, denom ) ) {
    ptr->numer_ = numer1;
    ptr->denom_ = denom1;
  }
  prc_ptr->drv1_ = 1;
}

static inline void upd_twap(
    pc_price_t *ptr, int64_t nslots, pc_qset_t *qs )
{
  pd_t px[1], conf[1];
  pd_new_scale( px, ptr->agg_.price_, ptr->expo_ );
  pd_new_scale( conf, ( int64_t )( ptr->agg_.conf_ ), ptr->expo_ );
  upd_ema( &ptr->twap_, px, conf, nslots, qs, ptr );
  upd_ema( &ptr->twac_, conf, conf, nslots, qs, ptr );
}

// compute weighted percentile
static void wgt_ptile(
  pd_t * const res
  , const pd_t * const prices, const pd_t * const weights, const uint32_t num
  , const pd_t * const ptile, pc_qset_t *qs )
{
  pd_t cumwgt[ PC_COMP_SIZE ];

  pd_t cumwgta[ 1 ], half[ 1 ];
  pd_new( cumwgta, 0, 0 );
  pd_new( half, 5, -1 );
  for ( uint32_t i = 0; i < num; ++i ) {
    const pd_t * const wptr = &weights[ i ];
    pd_t weight[ 1 ];
    pd_mul( weight, wptr, half );
    pd_add( &cumwgt[ i ], cumwgta, weight, qs->fact_ );
    pd_add( cumwgta, cumwgta, wptr, qs->fact_ );
  }

  uint32_t i = 0;
  for( ; i != num && pd_lt( &cumwgt[i], ptile, qs->fact_ ); ++i );
  if ( i == num ) {
    pd_set( res, &prices[num-1] );
  } else if ( i == 0 ) {
    pd_set( res, &prices[0] );
  } else {
    pd_t t1[1], t2[1];
    pd_sub( t1, &prices[i], &prices[i-1], qs->fact_ );
    pd_sub( t2, ptile, &cumwgt[i-1], qs->fact_ );
    pd_mul( t1, t1, t2 );
    pd_sub( t2, &cumwgt[i], &cumwgt[i-1], qs->fact_ );
    pd_div( t1, t1, t2 );
    pd_add( res, &prices[i-1], t1, qs->fact_ );
  }
}

// update aggregate price
static inline void upd_aggregate( pc_price_t *ptr, uint64_t slot )
{
  // only re-compute aggregate in next slot
  if ( slot <= ptr->agg_.pub_slot_ ) {
    return;
  }
  pc_qset_t *qs = qset_new( ptr->expo_ );

  // get number of slots from last published valid price
  int64_t agg_diff = ( int64_t )slot - ( int64_t )( ptr->last_slot_ );

  // copy previous price
  ptr->prev_slot_  = ptr->valid_slot_;
  ptr->prev_price_ = ptr->agg_.price_;
  ptr->prev_conf_  = ptr->agg_.conf_;

  // update aggregate details ready for next slot
  ptr->valid_slot_ = ptr->agg_.pub_slot_;// valid slot-time of agg. price
  ptr->agg_.pub_slot_ = slot;            // publish slot-time of agg. price

  uint32_t numv = 0;
  uint32_t vidx[ PC_COMP_SIZE ];
  // identify valid quotes and order them by price
  for ( uint32_t i = 0; i != ptr->num_; ++i ) {
    pc_price_comp_t *iptr = &ptr->comp_[i];
    // copy contributing price to aggregate snapshot
    iptr->agg_ = iptr->latest_;
    // add quote to sorted permutation array if it is valid
    int64_t slot_diff = ( int64_t )slot - ( int64_t )( iptr->agg_.pub_slot_ );
    if ( iptr->agg_.status_ == PC_STATUS_TRADING &&
         iptr->agg_.conf_ != 0UL &&
         iptr->agg_.price_ > 0L &&
         slot_diff >= 0 && slot_diff <= PC_MAX_SEND_LATENCY ) {
      vidx[numv++] = i;
    }
  }

  uint32_t nprcs = 0;
  int64_t prcs[ PC_COMP_SIZE * 2 ];
  for ( uint32_t i = 0; i < numv; ++i ) {
    pc_price_comp_t const *iptr = &ptr->comp_[ vidx[ i ] ];
    int64_t const price = iptr->agg_.price_;
    int64_t const conf = ( int64_t )( iptr->agg_.conf_ );
    prcs[ nprcs++ ] = price - conf;
    prcs[ nprcs++ ] = price + conf;
  }
  qsort_int64( prcs, nprcs );

  uint32_t numa = 0;
  uint32_t aidx[PC_COMP_SIZE];

  if ( nprcs ) {
    int64_t const mprc = ( prcs[ numv - 1 ] + prcs[ numv ] ) / 2;
    int64_t const lb = mprc / 5;
    int64_t const ub = ( mprc > LONG_MAX / 5 ) ? LONG_MAX : mprc * 5;

    for ( uint32_t i = 0; i < numv; ++i ) {
      uint32_t const idx = vidx[ i ];
      pc_price_comp_t const *iptr = &ptr->comp_[ idx ];
      int64_t const prc = iptr->agg_.price_;
      if ( prc >= lb && prc <= ub ) {
        uint32_t j = numa++;
        for( ; j > 0 && ptr->comp_[ aidx[ j - 1 ] ].agg_.price_ > prc; --j ) {
          aidx[ j ] = aidx[ j - 1 ];
        }
        aidx[ j ] = idx;
      }
    }
  }

  // zero quoters
  ptr->num_qt_ = numa;
  if ( numa == 0 || numa < ptr->min_pub_ || numa * 2 <= numv ) {
    ptr->agg_.status_ = PC_STATUS_UNKNOWN;
    return;
  }

  // update status and publish slot of last trading status price
  ptr->agg_.status_ = PC_STATUS_TRADING;
  ptr->last_slot_ = slot;

  // single quoter
  if ( numa == 1 ) {
    pc_price_comp_t *iptr = &ptr->comp_[aidx[0]];
    ptr->agg_.price_ = iptr->agg_.price_;
    ptr->agg_.conf_  = iptr->agg_.conf_;
    upd_twap( ptr, agg_diff, qs );
    return;
  }

  // assign quotes and compute weights
  pc_price_comp_t *pptr;
  pd_t price[1], conf[1], weight[1], one[1], wsum[1];
  pd_new( one, 100000000L, -8 );
  pd_new( wsum, 0, 0 );
  int64_t ldiff = INT64_MAX;
  pd_t *wptr = qs->weight_;
  for( uint32_t i=0;i != numa; ++i ) {
    pc_price_comp_t *iptr = &ptr->comp_[aidx[i]];
    // scale confidence interval by sqrt of slot age
    int64_t slot_diff = ( int64_t )slot - ( int64_t )( iptr->agg_.pub_slot_ );
    pd_t decay[1];
    pd_new( decay, qs->decay_[slot_diff], PC_EXP_DECAY );
    pd_new_scale( conf, ( int64_t )( iptr->agg_.conf_ ), ptr->expo_ );
    pd_mul( conf, conf, decay );

    // assign price and upper/lower bounds of price
    pd_new_scale( price, iptr->agg_.price_, ptr->expo_ );
    pd_set( &qs->iprice_[i], price );
    pd_add( &qs->uprice_[i], price, conf, qs->fact_ );
    pd_sub( &qs->lprice_[i], price, conf, qs->fact_ );

    // compute weight (1/(conf+nearest_neighbor))
    pd_set( &qs->weight_[i], conf );
    if ( i ) {
      int64_t idiff = iptr->agg_.price_ - pptr->agg_.price_;
      pd_new_scale( weight, idiff < ldiff ? idiff : ldiff, ptr->expo_ );
      pd_add( wptr, wptr, weight, qs->fact_ );
      pd_div( wptr, one, wptr );
      pd_add( wsum, wsum, wptr, qs->fact_ );
      ldiff = idiff;
      ++wptr;
    }
    pptr = iptr;
  }
  // compute weight for last quote
  pd_new_scale( weight, ldiff, ptr->expo_ );
  pd_add( wptr, wptr, weight, qs->fact_ );
  pd_div( wptr, one, wptr );
  pd_add( wsum, wsum, wptr, qs->fact_ );

  // bound weights at 1/sqrt(Nquotes) and redeistribute the remaining weight
  // among the remaining quoters proportional to their weights
  pd_t wmax[1], rnumer[1], rdenom[1];
  pd_set( rnumer, one );
  pd_new( rdenom, 0, 0 );
  // wmax = 1 / sqrt( numa )
  pd_new( wmax, numa, 0 );
  pd_sqrt( wmax, wmax, qs->fact_ );
  pd_div( wmax, one, wmax );
  for( uint32_t i=0;i != numa; ++i ) {
    wptr = &qs->weight_[i];
    pd_div( wptr, wptr, wsum );
    if ( pd_gt( wptr, wmax, qs->fact_ ) ) {
      pd_set( wptr, wmax );
      pd_sub( rnumer, rnumer, wmax, qs->fact_ );
      aidx[i] = 1;
    } else {
      pd_add( rdenom, rdenom, wptr, qs->fact_ );
      aidx[i] = 0;
    }
  }
  if ( rdenom->v_ ) {
    pd_div( rnumer, rnumer, rdenom );
  }
  for ( uint32_t i = 0; i != numa; ++i ) {
    wptr = &qs->weight_[ i ];
    if ( aidx[ i ] == 0 ) {
      pd_mul( wptr, wptr, rnumer );
    }
  }

  const pd_t half = { .e_ = -1, .v_ = 5 };

  // compute aggregate price as weighted median
  pd_t iprice[1], lprice[1], uprice[1], q3price[1], q1price[1], ptile[1];
  pd_new( ptile, 5, -1 );
  wgt_ptile( iprice, qs->iprice_, qs->weight_, numa, ptile, qs );
  pd_adjust( iprice, ptr->expo_, qs->fact_ );
  ptr->agg_.price_  = iprice->v_;

  // compute diff in weighted median between upper and lower price bounds
  pd_t prices[ PC_COMP_SIZE ];
  pd_t weights[ PC_COMP_SIZE ];
  // sort upper prices and weights
  for ( uint32_t i = 0; i < numa; ++i ) {
    uint32_t j = i;
    for ( ; j > 0 && pd_lt( &qs->uprice_[ i ], &prices[ j - 1 ], qs->fact_ ); --j ) {
      prices[ j ] = prices[ j - 1 ];
      weights[ j ] = weights[ j - 1 ];
    }
    prices[ j ] = qs->uprice_[ i ];
    weights[ j ] = qs->weight_[ i ];
  }
  wgt_ptile( uprice, prices, weights, numa, ptile, qs );
  // sort lower prices and weights
  for ( uint32_t i = 0; i < numa; ++i ) {
    uint32_t j = i;
    for ( ; j > 0 && pd_lt( &qs->lprice_[ i ], &prices[ j - 1 ], qs->fact_ ); --j ) {
      prices[ j ] = prices[ j - 1 ];
      weights[ j ] = weights[ j - 1 ];
    }
    prices[ j ] = qs->lprice_[ i ];
    weights[ j ] = qs->weight_[ i ];
  }
  wgt_ptile( lprice, prices, weights, numa, ptile, qs );

  pd_sub( uprice, uprice, lprice, qs->fact_ );
  pd_mul( uprice, uprice, &half );

  // compute weighted iqr of prices
  pd_new( ptile, 75, -2 );
  wgt_ptile( q3price, qs->iprice_, qs->weight_, numa, ptile, qs );
  pd_new( ptile, 25, -2 );
  wgt_ptile( q1price, qs->iprice_, qs->weight_, numa, ptile, qs );
  pd_sub( q3price, q3price, q1price, qs->fact_ );
  pd_mul( q3price, q3price, &half );

  // take confidence interval as larger
  pd_t *cptr = pd_gt( uprice, q3price, qs->fact_ ) ? uprice : q3price;
  pd_adjust( cptr, ptr->expo_, qs->fact_ );
  ptr->agg_.conf_ = ( uint64_t )( cptr->v_ );
  upd_twap( ptr, agg_diff, qs );
}

#ifdef __cplusplus
}
#endif

