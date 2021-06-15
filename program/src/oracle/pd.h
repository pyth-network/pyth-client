#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#define PD_MAX_NORMAL (6000-1)
#define PD_EXP_NORMAL -9
#define PD_SCALE9     (1000000000L)

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

static void pd_adjust( pd_t *r, pd_t *n, int e, uint64_t *p )
{
  int d = n->e_ - e;
  r->e_ = e;
  if ( d>0 )      r->v_ = n->v_*p[d];
  else if ( d<0 ) r->v_ = n->v_/p[-d];
  else            r->v_ = n->v_;
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

static void pd_add( pd_t *r, pd_t *n1, pd_t *n2, uint64_t *p )
{
  int d = n1->e_ - n2->e_;
  if ( d==0 ) {
    pd_new( r, n1->v_ + n2->v_, n1->e_ );
  } else if ( d>0 ) {
    if ( d<9 ) {
      pd_new( r, n1->v_*p[d] + n2->v_, n2->e_ );
    } else {
      pd_new( r, n1->v_*PD_SCALE9 + n2->v_/p[d-9], n1->e_-9);
    }
  } else {
    d = -d;
    if ( d<9 ) {
      pd_new( r, n1->v_ + n2->v_*p[d], n1->e_ );
    } else {
      pd_new( r, n1->v_/p[d-9] + n2->v_*PD_SCALE9, n2->e_-9 );
    }
  }
  pd_scale( r );
}

static void pd_sub( pd_t *r, pd_t *n1, pd_t *n2, uint64_t *p )
{
  int d = n1->e_ - n2->e_;
  if ( d==0 ) {
    pd_new( r, n1->v_ - n2->v_, n1->e_ );
  } else if ( d>0 ) {
    if ( d<9 ) {
      pd_new( r, n1->v_*p[d] - n2->v_, n2->e_ );
    } else {
      pd_new( r, n1->v_*PD_SCALE9 - n2->v_/p[d-9], n1->e_-9);
    }
  } else {
    d = -d;
    if ( d<9 ) {
      pd_new( r, n1->v_ - n2->v_*p[d], n1->e_ );
    } else {
      pd_new( r, n1->v_/p[d-9] - n2->v_*PD_SCALE9, n2->e_-9 );
    }
  }
  pd_scale( r );
}

static int pd_lt( pd_t *n1, pd_t *n2, uint64_t *p )
{
  pd_t r[1];
  pd_sub( r, n1, n2, p );
  return r->v_ < 0L;
}

static int pd_gt( pd_t *n1, pd_t *n2, uint64_t *p )
{
  pd_t r[1];
  pd_sub( r, n1, n2, p );
  return r->v_ > 0L;
}

static void pd_sqrt( pd_t *r, pd_t *val, uint64_t *f )
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

static uint64_t pd_normidx( pd_t *x, pd_t *m, pd_t *s, uint64_t *p )
{
  pd_t dx[1];
  pd_sub( dx, x, m, p );
  pd_div( dx, dx, s );
  int64_t idx = dx->v_;
  if ( idx<0 ) idx = -idx;
  int d = dx->e_ + 3;
  if ( d>0 ) idx *= p[d];
  else if ( d<0 ) idx /= p[-d];
  return idx < PD_MAX_NORMAL ? idx : PD_MAX_NORMAL;
}

#define pd_normal(r,x,m,s,p,t) \
  pd_new( r, t[pd_normidx(x,m,s,p)], PD_EXP_NORMAL );\
  pd_div( r, r, s )

static void pc_qs_new( pc_qs_t *q, pc_prm_t *prm )
{
  q->num_  = 0;
  q->fact_ = prm->fact_;
  q->norm_ = prm->norm_;
  pd_new( q->p_b_, 1, -2 );
  pd_new( q->nsig_, 5, 0 );
  pd_new( q->thr_, 9, 0 );
}

static void pc_qs_add( pc_qs_t *q, pd_t *minp, pd_t *sinp )
{
  uint64_t *p = q->fact_;
  pc_q_t *iq = &q->qt_[q->num_];
  // assign and adjust inputs
  iq->env_ = 0;
  pd_set( iq->m_, minp );
  pd_set( iq->s_, sinp );
  pd_mul( iq->s2_, iq->s_, iq->s_ );

  // update bounds for all quotes
  pd_sub( iq->l_, iq->m_, iq->s_, p );
  pd_add( iq->u_, iq->m_, iq->s_, p );
  pd_t nmu[1], tm[1], tp[1];
  pd_mul( nmu, q->nsig_, iq->s_ );
  pd_sub( tm, iq->m_, nmu, p );
  pd_add( tp, iq->m_, nmu, p );
  if ( q->num_ == 0 ) {
    pd_set( q->xmin_, tm );
    pd_set( q->xmax_, tp );
    q->exp_ = iq->l_->e_;
  } else {
    if ( pd_lt( tm, q->xmin_, p ) ) {
      pd_set( q->xmin_, tm );
    }
    if ( pd_gt( tp, q->xmax_, p ) ) {
      pd_set( q->xmax_, tp );
    }
  }
  if ( iq->l_->e_ < q->exp_ ) {
    q->exp_ = iq->l_->e_;
  }
  if ( iq->u_->e_ < q->exp_ ) {
    q->exp_ = iq->u_->e_;
  }
  // initialize group for this quote
  iq->id_ = iq->grp_ = 1UL<<q->num_++;
}

static void pc_qs_combine( pc_qs_t *q, pc_q_t *q1, pc_q_t *q2 )
{
  pd_t w[1], t1[1], t2[1], t3[1], t4[1];
  uint64_t *p = q->fact_;
  pd_add( t3, q1->s2_, q2->s2_, p );
  pd_sqrt( t4, t3, p );
  pd_normal( t1, q1->m_, q2->m_, t4, p, q->norm_ );
  pd_mul( t1, t1, q1->a_ );
  pd_mul( w, t1, q2->a_ );

  pd_mul( t1, q1->m_, q2->s2_ );
  pd_mul( t2, q2->m_, q1->s2_ );
  pd_add( t1, t1, t2, p );
  pd_div( q1->m_, t1, t3 );

  pd_mul( t1, q1->s_, q2->s_ );
  pd_div( q1->s_, t1, t4 );
  pd_mul( q1->s2_, q1->s_, q1->s_ );

  pd_set( q1->a_, w );
  pd_mul( q1->b_, q1->b_, q2->b_ );
}

static void pc_qs_add_env( pc_qs_t *q, pc_q_t *iq )
{
  if ( iq->env_ == 0 ) {
    iq->env_ = 1;
    q->ep_[q->nenv_++] = iq;
  }
}

static void pc_qs_identify_groups( pc_qs_t *q )
{
  // identify enveloping and overlapping quotes
  pd_t one[1], a[1], b[1];
  uint64_t *p = q->fact_;
  pd_new( one, 1, 0 );
  pd_sub( a, one, q->p_b_, p );
  pd_sub( b, q->xmax_, q->xmin_, p );
  pd_div( b, q->p_b_, b );

  // identify pair-wise overlap
  q->nenv_ = q->ngrp_ = 0;
  for( unsigned i=0; i != q->num_; ++i ) {
    pc_q_t *iq = &q->qt_[i];
    pd_set( iq->a_, a );
    pd_set( iq->b_, b );
    for( unsigned j=i+1; j != q->num_; ++j ) {
      pc_q_t *jq = &q->qt_[j];
      if ( iq->l_->v_ < jq->l_->v_ && iq->u_->v_ > jq->u_->v_ ) {
        // i envelops j
        pc_qs_add_env( q, iq );
      } else if ( jq->l_->v_ < iq->l_->v_ && jq->u_->v_ > iq->u_->v_ ) {
        // j envelops i
        pc_qs_add_env( q, jq );
      } else {
        // check if they overlap rather than envelop each other
        pd_t dmu[1], sig[1], ratio[1];
        pd_sub( dmu, iq->m_, jq->m_, p );
        pd_mul( dmu, dmu, dmu );
        pd_add( sig, iq->s2_, jq->s2_, p );
        pd_div( ratio, dmu, sig );
        if ( pd_lt( ratio, q->thr_, p ) ) {
          iq->grp_ |= jq->id_;
        }
      }
    }
  }

  // merge overlapping groups
  for( unsigned i=0; i != q->num_; ++i ) {
    pc_q_t *iq = &q->qt_[i];
    if ( iq->grp_ ) {
      for( unsigned j=i+1; j != q->num_; ++j ) {
        pc_q_t *jq = &q->qt_[j];
        if ( iq->grp_ & jq->grp_ ) {
          iq->grp_ |= jq->grp_;
          jq->grp_ = 0UL;
        }
      }
    }
  }

  // combine groups
  for( unsigned i=0; i != q->num_; ++i ) {
    pc_q_t *iq = &q->qt_[i];
    if ( iq->grp_ && iq->env_ == 0) {
      // combine all quotes within group
      q->gp_[q->ngrp_++] = iq;
      for( unsigned j=i+1; j != q->num_; ++j ) {
        pc_q_t *jq = &q->qt_[j];
        if ( iq->grp_ & jq->id_ ) {
          pc_qs_combine( q, iq, jq );
        }
      }
      // apply enveloping quotes to combined quote
      for( unsigned j=0; j != q->nenv_; ++j ) {
        pc_q_t *eq = q->ep_[j];
        pd_t adj[1];
        pd_normal( adj, iq->m_, eq->m_, eq->s_, p, q->norm_ );
        pd_mul( adj, adj, eq->a_ );
        pd_add( adj, adj, eq->b_, p );
        pd_mul( iq->a_, iq->a_, adj );
      }
    }
  }
}

static void pc_qs_calc( pc_qs_t *q )
{
  // align lower/upper bounds
  for( unsigned i=0; i != q->num_; ++i ) {
    pc_q_t *iq = &q->qt_[i];
    pd_adjust( iq->l_, iq->l_, q->exp_, q->fact_ );
    pd_adjust( iq->u_, iq->u_, q->exp_, q->fact_ );
  }

  // identify enveloping and overlapping quotes
  pc_qs_identify_groups( q );

  // short cut if only one combined quote
  if ( q->ngrp_ == 1 ) {
    pc_q_t *gq = q->gp_[0];
    pd_set( q->m_, gq->m_ );
    pd_set( q->s_, gq->s_ );
    return;
  }

  // aggregate combined quotes into final quote
  pd_t num[1], denom[1], t1[1], t2[1];
  uint64_t *p = q->fact_;
  for( unsigned gid=0; gid != q->ngrp_; ++gid ) {
    pc_q_t *gq = q->gp_[gid];
    pd_div( t1, gq->a_, gq->b_ );
    pd_mul( t2, t1, gq->m_ );
    if ( gid ) {
      pd_add( num, num, t2, p );
      pd_add( denom, denom, t1, p );
    } else {
      pd_set( num, t2 );
      pd_set( denom, t1 );
    }
  }
  pd_sub( t1, q->xmax_, q->xmin_, p );
  pd_add( denom, denom, t1, p );
  pd_mul( t1, q->xmax_, q->xmax_ );
  pd_mul( t2, q->xmin_, q->xmin_ );
  pd_sub( t1, t1, t2, p );
  pd_new( t2, 5, -1 );
  pd_mul( t1, t1, t2 );
  pd_add( num, num, t1, p );
  pd_div( q->m_, num, denom );

  for( unsigned gid=0; gid != q->ngrp_; ++gid ) {
    pc_q_t *gq = q->gp_[gid];
    pd_div( t1, gq->a_, gq->b_ );
    pd_sub( t2, gq->m_, q->m_, p );
    pd_mul( t2, t2, t2 );
    pd_add( t2, t2, gq->s2_, p );
    pd_mul( t1, t1, t2 );
    if ( gid ) {
      pd_add( num, num, t1, p );
    } else {
      pd_set( num, t1 );
    }
  }
  pd_t mmax[1], mmin[1];
  pd_sub( mmax, q->xmax_, q->m_, p );
  pd_sub( mmin, q->xmin_, q->m_, p );
  pd_mul( t1, mmax, mmax );
  pd_mul( t1, t1, mmax );
  pd_mul( t2, mmin, mmin );
  pd_mul( t2, t2, mmin );
  pd_sub( t1, t1, t2, p );
  pd_new( t2, 3, 0 );
  pd_div( t1, t1, t2 );
  pd_add( num, num, t1, p );
  pd_div( t1, num, denom );
  pd_sqrt( q->s_, t1, p );
}


#ifdef __cplusplus
}
#endif
