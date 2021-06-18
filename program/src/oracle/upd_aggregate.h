#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#define PD_SCALE9     (1000000000L)
#define PD_EMA_MAX_DIFF 4145     // maximum slots before reset
#define PD_EMA_EXPO     -9       // exponent of temporary storage
#define PD_EMA_DECAY   (-117065) // 1e9*-log(2)/5921

#define pd_new( n,v,e ) {(n)->v_=v;(n)->e_=e;}
#define pd_set( r,n ) (r)[0] = (n)[0]
#define pd_new_scale(n,v,e) {(n)->v_=v;(n)->e_=e;pd_scale(n);}

static void pd_scale( pd_t *n )
{
  int neg = n->v_ < 0L;
  uint64_t v = neg?-n->v_:n->v_;
  for( ;v&0xfffffffff0000000UL; v/= 10UL, ++n->e_ );
  n->v_ = neg ? -v : v;
}

static void pd_adjust( pd_t *n, int e, const uint64_t *p )
{
  int neg = n->v_ < 0L;
  uint64_t v = neg?-n->v_:n->v_;
  int d = n->e_ - e;
  if ( d>0 ) v *= p[d]; else if ( d<0 ) v /= p[-d];
  pd_new( n, neg ? -v : v, e );
}

static void pd_mul( pd_t *r, pd_t *n1, pd_t *n2 )
{
  r->v_ = n1->v_ * n2->v_;
  r->e_ = n1->e_ + n2->e_;
  pd_scale( r );
}

static void pd_div( pd_t *r, pd_t *n1, pd_t *n2 )
{
  if ( n1->v_ == 0 ) { pd_set( r, n1 ); return; }
  int64_t v1 = n1->v_, v2 = n2->v_;
  int neg1 = v1<0L, neg2 = v2<0L, m=0;
  if ( neg1 ) v1 = -v1;
  if ( neg2 ) v2 = -v2;
  for( ;0L == (v1&0xfffffffff0000000); v1*= 10L, ++m );
  r->v_ = (v1 * PD_SCALE9) / v2;
  if ( neg1 ) r->v_ = -r->v_;
  if ( neg2 ) r->v_ = -r->v_;
  r->e_ = n1->e_ - n2->e_ - m - 9;
  pd_scale( r );
}

static void pd_add( pd_t *r, pd_t *n1, pd_t *n2, const uint64_t *p )
{
  int d = n1->e_ - n2->e_;
  if ( d==0 ) {
    pd_new( r, n1->v_ + n2->v_, n1->e_ );
  } else if ( d>0 ) {
    if ( d<9 ) {
      pd_new( r, n1->v_*p[d] + n2->v_, n2->e_ );
    } else if ( d < PC_FACTOR_SIZE+9 ) {
      pd_new( r, n1->v_*PD_SCALE9 + n2->v_/p[d-9], n1->e_-9);
    } else {
      pd_set( r, n1 );
    }
  } else {
    d = -d;
    if ( d<9 ) {
      pd_new( r, n1->v_ + n2->v_*p[d], n1->e_ );
    } else if ( d < PC_FACTOR_SIZE+9 ) {
      pd_new( r, n1->v_/p[d-9] + n2->v_*PD_SCALE9, n2->e_-9 );
    } else {
      pd_set( r, n2 );
    }
  }
  pd_scale( r );
}

static void pd_sub( pd_t *r, pd_t *n1, pd_t *n2, const uint64_t *p )
{
  int d = n1->e_ - n2->e_;
  if ( d==0 ) {
    pd_new( r, n1->v_ - n2->v_, n1->e_ );
  } else if ( d>0 ) {
    if ( d<9 ) {
      pd_new( r, n1->v_*p[d] - n2->v_, n2->e_ );
    } else if ( d < PC_FACTOR_SIZE+9 ) {
      pd_new( r, n1->v_*PD_SCALE9 - n2->v_/p[d-9], n1->e_-9);
    } else {
      pd_set( r, n1 );
    }
  } else {
    d = -d;
    if ( d<9 ) {
      pd_new( r, n1->v_ - n2->v_*p[d], n1->e_ );
    } else if ( d < PC_FACTOR_SIZE+9 ) {
      pd_new( r, n1->v_/p[d-9] - n2->v_*PD_SCALE9, n2->e_-9 );
    } else {
      pd_new( r, -n2->v_, n2->e_ );
    }
  }
  pd_scale( r );
}

static int pd_lt( pd_t *n1, pd_t *n2, const uint64_t *p )
{
  pd_t r[1];
  pd_sub( r, n1, n2, p );
  return r->v_ < 0L;
}

static int pd_gt( pd_t *n1, pd_t *n2, const uint64_t *p )
{
  pd_t r[1];
  pd_sub( r, n1, n2, p );
  return r->v_ > 0L;
}

static void pd_sqrt( pd_t *r, pd_t *val, const uint64_t *f )
{
  pd_t t[1], x[1], hlf[1];
  pd_set( t, val );
  pd_new( r, 1, 0 );
  pd_add( x, t, r, f );
  pd_new( hlf, 5, -1 );
  pd_mul( x, x, hlf );
  for(;;) {
    pd_div( r, t, x );
    pd_add( r, r, x, f );
    pd_mul( r, r, hlf );
    if ( x->v_ == r->v_ ) {
      break;
    }
    pd_set( x, r );
  }
}

// initialize quote-set temporary data in heap area
static pc_qset_t *qset_new()
{
  // allocate off heap
  pc_qset_t *qs = (pc_qset_t*)PC_HEAP_START;

  // sqrt of numbers 1 to 25 for decaying conf. interval based on slot delay
  qs->decay_[0]  = 1000000000UL;
  qs->decay_[1]  = 1000000000UL;
  qs->decay_[2]  = 1414213562UL;
  qs->decay_[3]  = 1732050807UL;
  qs->decay_[4]  = 2000000000UL;
  qs->decay_[5]  = 2236067977UL;
  qs->decay_[6]  = 2449489742UL;
  qs->decay_[7]  = 2645751311UL;
  qs->decay_[8]  = 2828427124UL;
  qs->decay_[9]  = 3000000000UL;
  qs->decay_[10] = 3162277660UL;
  qs->decay_[11] = 3316624790UL;
  qs->decay_[12] = 3464101615UL;
  qs->decay_[13] = 3605551275UL;
  qs->decay_[14] = 3741657386UL;
  qs->decay_[15] = 3872983346UL;
  qs->decay_[16] = 4000000000UL;
  qs->decay_[17] = 4123105625UL;
  qs->decay_[18] = 4242640687UL;
  qs->decay_[19] = 4358898943UL;
  qs->decay_[20] = 4472135954UL;
  qs->decay_[21] = 4582575694UL;
  qs->decay_[22] = 4690415759UL;
  qs->decay_[23] = 4795831523UL;
  qs->decay_[24] = 4898979485UL;
  qs->decay_[25] = 5000000000UL;

  // powers of 10 for use in decimal arithmetic scaling
  qs->fact_[0]   = 1UL;
  qs->fact_[1]   = 10UL;
  qs->fact_[2]   = 100UL;
  qs->fact_[3]   = 1000UL;
  qs->fact_[4]   = 10000UL;
  qs->fact_[5]   = 100000UL;
  qs->fact_[6]   = 1000000UL;
  qs->fact_[7]   = 10000000UL;
  qs->fact_[8]   = 100000000UL;
  qs->fact_[9]   = 1000000000UL;
  qs->fact_[10]  = 10000000000UL;
  qs->fact_[11]  = 100000000000UL;
  qs->fact_[12]  = 1000000000000UL;
  qs->fact_[13]  = 10000000000000UL;
  qs->fact_[14]  = 100000000000000UL;
  qs->fact_[15]  = 1000000000000000UL;
  qs->fact_[16]  = 10000000000000000UL;
  qs->fact_[17]  = 100000000000000000UL;

  return qs;
}

static void upd_ema(
    pc_ema_t *ptr, int64_t v, int e, int64_t nslot, pc_qset_t *qs)
{
  pd_t val[1], numer[1], denom[1], decay[1], diff[1], one[1];
  pd_new( one, PD_SCALE9, PD_EMA_EXPO );
  pd_new_scale( val, v, e );
  pd_new_scale( numer, ptr->numer_, PD_EMA_EXPO );
  pd_new_scale( denom, ptr->denom_, PD_EMA_EXPO );
  if ( nslot > PD_EMA_MAX_DIFF ) {
    ptr->val_   = v;
    pd_adjust( val, PD_EMA_EXPO, qs->fact_ );
    ptr->numer_ = val->v_;
    ptr->denom_ = one->v_;
    return;
  }
  // compute decay factor
  pd_new( diff, nslot, 0 );
  pd_new( decay, PD_EMA_DECAY, PD_EMA_EXPO );
  pd_mul( decay, decay, diff );
  pd_add( decay, decay, one, qs->fact_ );

  // compute numerator/denominator and new value from decay factor
  pd_mul( numer, numer, decay );
  pd_add( numer, numer, val, qs->fact_ );
  pd_mul( denom, denom, decay );
  pd_add( denom, denom, one, qs->fact_ );
  pd_div( val, numer, denom );

  // adjust and store results
  pd_adjust( val, e, qs->fact_ );
  pd_adjust( numer, PD_EMA_EXPO, qs->fact_ );
  pd_adjust( denom, PD_EMA_EXPO, qs->fact_ );
  ptr->val_ = val->v_;
  ptr->numer_ = numer->v_;
  ptr->denom_ = denom->v_;
}

static inline void upd_twap(
    pc_price_t *ptr, int64_t nslots, pc_qset_t *qs )
{
  upd_ema( &ptr->twap_, ptr->agg_.price_, ptr->expo_, nslots, qs );
  upd_ema( &ptr->twac_, ptr->agg_.conf_, ptr->expo_, nslots, qs );
}

// compute weighted percentile
static void wgt_ptile( pd_t *res, pd_t *prices, pd_t *ptile, pc_qset_t *qs )
{
  pd_t *cumwgt = qs->cumwgt_;
  uint32_t i =0, num = qs->num_;
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
static void upd_aggregate( pc_price_t *ptr, uint64_t slot )
{
  // only re-compute aggregate in next slot
  if ( slot <= ptr->agg_.pub_slot_ ) {
    return;
  }
  pc_qset_t *qs = qset_new();

  // get number of slots from last published valid price
  int64_t agg_diff = slot - ptr->last_slot_;

  // copy previous price
  ptr->prev_slot_  = ptr->valid_slot_;
  ptr->prev_price_ = ptr->agg_.price_;
  ptr->prev_conf_  = ptr->agg_.conf_;

  // update aggregate details ready for next slot
  ptr->valid_slot_ = ptr->agg_.pub_slot_;// valid slot-time of agg. price
  ptr->agg_.pub_slot_ = slot;            // publish slot-time of agg. price

  // identify valid quotes and order them by price
  uint32_t numa = 0 ;
  uint32_t aidx[PC_COMP_SIZE];
  for( uint32_t i=0; i != ptr->num_; ++i ) {
    pc_price_comp_t *iptr = &ptr->comp_[i];
    // copy contributing price to aggregate snapshot
    iptr->agg_ = iptr->latest_;
    // add quote to sorted permutation array if it is valid
    int64_t slot_diff = slot - iptr->agg_.pub_slot_;
    if ( iptr->agg_.status_ == PC_STATUS_TRADING &&
         iptr->agg_.conf_ != 0UL &&
         slot_diff >= 0 && slot_diff <= PC_MAX_SEND_LATENCY ) {
      int64_t ipx = iptr->agg_.price_;
      uint32_t j = numa++;
      for( ; j > 0 && ptr->comp_[aidx[j-1]].agg_.price_ > ipx; --j ) {
        aidx[j] = aidx[j-1];
      }
      aidx[j] = i;
    }
  }

  // zero quotes
  if ( numa == 0 ) {
    ptr->agg_.status_ = PC_STATUS_UNKNOWN;
    upd_twap( ptr, agg_diff, qs );
    return;
  }

  // update status and publish slot of last trading status price
  ptr->agg_.status_ = PC_STATUS_TRADING;
  ptr->last_slot_ = slot;
  if ( numa == 1 ) {
    int64_t price = ptr->comp_[aidx[0]].agg_.price_;
    ptr->agg_.price_ = price;
    ptr->agg_.conf_ = (price>0?price:-price)/2L;
    upd_twap( ptr, agg_diff, qs );
    return;
  }

  // assign quotes and compute weights
  pc_price_comp_t *pptr = NULL;
  pd_t price[1], conf[1], weight[1], one[1], wsum[1];
  pd_new( one, 100000000L, -8 );
  pd_new( wsum, 0, 0 );
  int64_t ldiff = INT64_MAX;
  qs->num_  = numa;
  pd_t *wptr = qs->weight_;
  for( uint32_t i=0;i != numa; ++i ) {
    pc_price_comp_t *iptr = &ptr->comp_[aidx[i]];
    // scale confidence interval by sqrt of slot age
    int64_t slot_diff = slot - iptr->agg_.pub_slot_;
    pd_t decay[1];
    pd_new( decay, qs->decay_[slot_diff], PC_EXP_DECAY );
    pd_new_scale( conf, iptr->agg_.conf_, ptr->expo_ );
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
  pd_t wmax[1], rnumer[1], rdenom[1], half[1], cumwgt[1];
  pd_set( rnumer, one );
  pd_new( rdenom, 0, 0 );
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
  pd_new( cumwgt, 0, 0 );
  pd_new( half, 5, -1 );
  for( uint32_t i=0;i != numa; ++i ) {
    wptr = &qs->weight_[i];
    if ( aidx[i] == 0 ) {
      pd_mul( wptr, wptr, rnumer );
    }
    pd_mul( weight, wptr, half );
    pd_add( &qs->cumwgt_[i], cumwgt, weight, qs->fact_ );
    pd_add( cumwgt, cumwgt, wptr, qs->fact_ );
  }

  // compute aggregate price as weighted median
  pd_t iprice[1], lprice[1], uprice[1], q3price[1], q1price[1], ptile[1];
  pd_new( ptile, 5, -1 );
  wgt_ptile( iprice, qs->iprice_, ptile, qs );
  pd_adjust( iprice, ptr->expo_, qs->fact_ );
  ptr->agg_.price_  = iprice->v_;

  // compute diff in weighted median between upper and lower price bounds
  wgt_ptile( uprice, qs->uprice_, ptile, qs );
  wgt_ptile( lprice, qs->lprice_, ptile, qs );
  pd_sub( uprice, uprice, lprice, qs->fact_ );
  pd_mul( uprice, uprice, half );

  // compute weighted iqr of prices
  pd_new( ptile, 75, -2 );
  wgt_ptile( q3price, qs->iprice_, ptile, qs );
  pd_new( ptile, 25, -2 );
  wgt_ptile( q1price, qs->iprice_, ptile, qs );
  pd_sub( q3price, q3price, q1price, qs->fact_ );
  pd_mul( q3price, q3price, half );

  // take confidence interval as larger
  pd_t *cptr = pd_gt( uprice, q3price, qs->fact_ ) ? uprice : q3price;
  pd_adjust( cptr, ptr->expo_, qs->fact_ );
  ptr->agg_.conf_ = cptr->v_;
  upd_twap( ptr, agg_diff, qs );
}

#ifdef __cplusplus
}
#endif

