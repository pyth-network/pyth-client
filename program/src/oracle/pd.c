#include "pd.h"

void pd_scale( pd_t* const n )
{
  int const neg = n->v_ < 0L;
  int64_t v = neg ? -n->v_ : n->v_; // make v positive for loop condition
  for( ; v >= ( 1L << 28 ); v /= 10L, ++n->e_ );
  n->v_ = neg ? -v : v;
}

bool pd_store( int64_t* const r, const pd_t* const n )
{
  int64_t v = n->v_;
  int32_t e = n->e_;
  while ( v <  -( 1L << 58 ) ) {
    v /= 10;
    ++e;
  }
  while ( v > ( 1L << 58 ) - 1 ) {
    v /= 10;
    ++e;
  }
  while ( e < -( 1 << ( EXP_BITS - 1 ) ) ) {
    v /= 10;
    ++e;
  }
  while ( e > ( 1 << ( EXP_BITS - 1 ) ) - 1 ) {
    v *= 10;
    if ( v < -( 1L << 58 ) || v > ( 1L << 58 ) - 1 ) {
      return false;
    }
    --e;
  }
  *r = ( v << EXP_BITS ) | ( e & EXP_MASK );
  return true;
}

void pd_load( pd_t* const r, int64_t const n )
{
  pd_new( r, n >> EXP_BITS, ( ( n & EXP_MASK ) << 59 ) >> 59 );
  pd_scale( r );
}

void pd_adjust( pd_t* const n, int const e, const int64_t* const p )
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

void pd_mul( pd_t* const r, const pd_t* const n1, const pd_t* const n2 )
{
  r->v_ = n1->v_ * n2->v_;
  r->e_ = n1->e_ + n2->e_;
  pd_scale( r );
}

void pd_div( pd_t* const r, pd_t* const n1, pd_t* const n2 )
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

void pd_add(
  pd_t* const r,
  const pd_t* const n1,
  const pd_t* const n2,
  const int64_t* const p )
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

void pd_sub(
  pd_t* const r,
  const pd_t* const n1,
  const pd_t* const n2,
  const int64_t* const p )
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

int pd_lt(
  const pd_t* const n1,
  const pd_t* const n2,
  const int64_t* const p )
{
  pd_t r[1];
  pd_sub( r, n1, n2, p );
  return r->v_ < 0L;
}

int pd_gt(
  const pd_t* const n1,
  const pd_t* const n2,
  const int64_t* const p )
{
  pd_t r[1];
  pd_sub( r, n1, n2, p );
  return r->v_ > 0L;
}

void pd_sqrt(
  pd_t* const r,
  pd_t* const val,
  const int64_t* const f )
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
