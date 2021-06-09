#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <oracle/oracle.h>
#include <oracle/pd.h>
#include <oracle/normal.h>
#include <pc/mem_map.hpp>
#include <pc/jtree.hpp>
#include <pc/net_socket.hpp>
#include <math.h>
#include <iostream>

using namespace pc;

int usage()
{
  std::cerr << "usage: test_qset <test.json>" << std::endl;
  return 1;
}

void init_prm( pc_prm_t *p )
{
  // scale factors
  uint64_t m = 1UL;
  for( uint32_t i=0;i != PC_FACTOR_SIZE; ++i ) {
    p->fact_[i] = m;
    m *= 10UL;
  }
  // square roots
  pd_t val[1];
  for( uint32_t i=0; i != PC_MAX_SEND_LATENCY; ++i ) {
    pd_new( val, i+1, 0 );
    pd_sqrt( val, val, p->fact_ );
    pd_adjust( val, val, -9, p->fact_ );
    p->cdecay_[i] = val->v_;
  }
  // normal dist. lookups
  for( unsigned i=0;i != PC_NORMAL_SIZE;++i ) {
    p->norm_[i] = ntable_[i];
  }
}

int main( int argc,char** argv )
{
  if ( argc < 2 ) {
    return usage();
  }
  // initialize param table
  pc_prm_t prm[1];
  init_prm( prm );

  // construct calculator
  pc_qs_t qs[1];
  pd_t m[1], s[1];
  pc_qs_new( qs, prm );

  pc_drv_t qd[1];
  pc_drv_new( qd, prm );
  pd_t twap[1], avol[1], prev[1];

  // read scenario file
  mem_map mf;
  std::string file = argv[1];
  mf.set_file(file );
  if ( !mf.init() ) {
    std::cerr << "test_qset: failed to read file[" << file << "]"
              << std::endl;
    return 1;
  }
  jtree pt;
  pt.parse( mf.data(), mf.size() );
  if ( !pt.is_valid() ) {
    std::cerr << "test_qset: failed to parse file[" << file << "]"
              << std::endl;
    return 1;
  }

  // previous values of price, vol and twap
  int expo = pt.get_int( pt.find_val( 1, "exponent" ) );
  uint32_t vt = pt.find_val( 1, "derived" );
  if ( vt ) {
    int64_t iprev = pt.get_int( pt.find_val( vt,  "prev_price"  ) );
    int64_t itwap = pt.get_int( pt.find_val( vt,  "twap"  ) );
    int64_t iavol = pt.get_int( pt.find_val( vt,  "avol"  ) );
    pd_new_scale( prev, iprev, expo );
    pd_new_scale( twap, itwap, expo );
    pd_new_scale( avol, iavol, expo );
  } else {
    pd_new( prev, 0, 0 );
    pd_new( twap, 0, 0 );
    pd_new( avol, 0, 0 );
  }

  // construct quotes
  uint32_t qt =pt.find_val( 1, "quotes" );
  for( uint32_t it = pt.get_first( qt ); it; it = pt.get_next( it ) ) {
    int64_t px = pt.get_int( pt.find_val( it,  "price"  ) );
    int64_t conf = pt.get_uint( pt.find_val( it, "conf" ) );
    int64_t sdiff = pt.get_int( pt.find_val( it, "slot_diff" ) );
    pd_new( m, px, expo );
    pd_new( s, conf, expo );

    // multiply by sqrt(slot_diff) since this is what upd_aggregate does
    pd_t t[1];
    pd_new( t, prm->cdecay_[sdiff>0?sdiff-1:0], -9 );
    pd_mul( s, s, t );

    // add quote
    pc_qs_add( qs, m, s );
  }
  pc_qs_calc( qs );
  pc_twap_calc( qd, twap, qs->m_ );
  pc_avol_calc( qd, avol, qs->m_, prev );

  // scale results
  expo = std::min( qs->m_->e_, qs->s_->e_ );
  if ( expo <= 0 ) expo = -9;
  pd_adjust( qs->m_, qs->m_, expo, prm->fact_ );
  pd_adjust( qs->s_, qs->s_, expo, prm->fact_ );
  pd_adjust( twap, twap, expo, prm->fact_ );
  pd_adjust( avol, avol, expo, prm->fact_ );

  // generate result
  json_wtr jw;
  jw.add_val( json_wtr::e_obj );
  jw.add_key( "exponent", (int64_t)expo );
  jw.add_key( "price", qs->m_->v_ );
  jw.add_key( "conf", qs->s_->v_ );
  jw.add_key( "twap", twap->v_ );
  jw.add_key( "avol", avol->v_ );
  jw.pop();
  net_buf *hd, *tl;
  jw.detach( hd, tl );
  while( hd ) {
    net_buf *nxt = hd->next_;
    std::cout.write( hd->buf_, hd->size_ );
    hd->dealloc();
    hd = nxt;
  }
  std::cout << std::endl;

  return 0;
}
