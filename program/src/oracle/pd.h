#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#define PD_SCALE9     (1000000000L)
#define PD_EMA_MAX_DIFF 4145     // maximum slots before reset
#define PD_EMA_EXPO     -9       // exponent of temporary storage
#define PD_EMA_DECAY   (-117065) // 1e9*-log(2)/5921
#define PC_FACTOR_SIZE       18

#define pd_new( n,v,e ) {(n)->v_=v;(n)->e_=e;}
#define pd_set( r,n ) (r)[0] = (n)[0]
#define pd_new_scale(n,v,e) {(n)->v_=v;(n)->e_=e;pd_scale(n);}

// decimal number format
typedef struct pd
{
  int32_t  e_;
  int64_t  v_;
} pd_t;

static void pd_scale( pd_t *n )
{
  int const neg = n->v_ < 0L;
  int64_t v = neg ? -n->v_ : n->v_; // make v positive for loop condition
  for( ; v >= ( 1L << 28 ); v /= 10L, ++n->e_ );
  n->v_ = neg ? -v : v;
}

static void pd_adjust( pd_t *n, int e, const int64_t *p )
{
  int64_t v = n->v_;
  int d = n->e_ - e;
  if ( d > 0 ) {
    v *= p[ d ];
  }
  else if ( d < 0 ) {
    v /= p[ -d ];
  }
  pd_new( n, v, e );
}

static void pd_mul( pd_t *r, const pd_t *n1, const pd_t *n2 )
{
  r->v_ = n1->v_ * n2->v_;
  r->e_ = n1->e_ + n2->e_;
  pd_scale( r );
}

static void pd_div( pd_t *r, pd_t *n1, pd_t *n2 )
{
  if ( n1->v_ == 0 ) { pd_set( r, n1 ); return; }
  int64_t v1 = n1->v_, v2 = n2->v_;
  int neg1 = v1 < 0L, neg2 = v2 < 0L, m = 0;
  if ( neg1 ) v1 = -v1;
  if ( neg2 ) v2 = -v2;
  for( ; 0UL == ( ( uint64_t )v1 & 0xfffffffff0000000UL ); v1 *= 10L, ++m );
  r->v_ = ( v1 * PD_SCALE9 ) / v2;
  if ( neg1 ) r->v_ = -r->v_;
  if ( neg2 ) r->v_ = -r->v_;
  r->e_ = n1->e_ - n2->e_ - m - 9;
  pd_scale( r );
}

static void pd_add( pd_t *r, const pd_t *n1, const pd_t *n2, const int64_t *p )
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

static void pd_sub( pd_t *r, const pd_t *n1, const pd_t *n2, const int64_t *p )
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

static int pd_lt( const pd_t *n1, const pd_t *n2, const int64_t *p )
{
  pd_t r[1];
  pd_sub( r, n1, n2, p );
  return r->v_ < 0L;
}

static int pd_gt( const pd_t *n1, const pd_t *n2, const int64_t *p )
{
  pd_t r[1];
  pd_sub( r, n1, n2, p );
  return r->v_ > 0L;
}

static void pd_sqrt( pd_t *r, pd_t *val, const int64_t *f )
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

#ifdef __cplusplus
}
#endif

