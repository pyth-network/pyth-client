char heap_start[8192];
#define PC_HEAP_START (heap_start)

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <oracle/oracle.h>
#include <oracle/upd_aggregate.h>
#include <pc/mem_map.hpp>
#include <pc/jtree.hpp>
#include <pc/net_socket.hpp>
#include "test_error.hpp"
#include <math.h>
#include <iostream>

using namespace pc;

int usage()
{
  std::cerr << "usage: test_qset <test.json>" << std::endl;
  return 1;
}

void test_pd()
{
  const uint64_t dec_fact[] = {
    1UL, 10UL, 100UL, 1000UL, 10000UL, 100000UL, 1000000UL, 10000000UL,
    100000000UL, 1000000000UL, 10000000000UL, 100000000000UL, 1000000000000UL,
    10000000000000UL, 100000000000000UL, 1000000000000000UL, 10000000000000000UL,
    100000000000000000UL
  };
  // construct calculator
  pd_t m[1], s[1];
  pd_new_scale( m, 100, 0 );
  pd_new_scale( s, 10, 0 );

  pd_t r[1], n1[1], n2[1];
  // test add/subtract numbers close in magnitude
  pd_new( n1, 123, 4 );
  pd_new( n2, 142, 1 );
  pd_add( r, n1, n2, dec_fact );
  PC_TEST_CHECK( r->v_ == 123142 && r->e_ == 1 );
  pd_add( r, n2, n1, dec_fact );
  PC_TEST_CHECK( r->v_ == 123142 && r->e_ == 1 );
  pd_sub( r, n1, n2, dec_fact );
  PC_TEST_CHECK( r->v_ == 122858 && r->e_ == 1 );
  pd_sub( r, n2, n1, dec_fact );
  PC_TEST_CHECK( r->v_ == -122858 && r->e_ == 1 );

  // test add/subtract numbers close in but less than zero
  pd_new( n1, 123, -4 );
  pd_new( n2, 142, -1 );
  pd_add( r, n1, n2, dec_fact );
  PC_TEST_CHECK( r->v_ == 142123 && r->e_ == -4 );
  pd_add( r, n2, n1, dec_fact );
  PC_TEST_CHECK( r->v_ == 142123 && r->e_ == -4 );
  pd_sub( r, n1, n2, dec_fact );
  PC_TEST_CHECK( r->v_ == -141877 && r->e_ == -4 );
  pd_sub( r, n2, n1, dec_fact );
  PC_TEST_CHECK( r->v_ == 141877 && r->e_ == -4 );

  // test add/subtract with larger didec_factdec_fact in magnitude but not too big
  pd_new( n1, 123, 10 );
  pd_new( n2, 142567, 1 );
  pd_add( r, n1, n2, dec_fact );
  // scaled back to 9 digits odec_fact precision
  PC_TEST_CHECK( r->v_ == 123000142 && r->e_ == 4 );
  pd_add( r, n2, n1, dec_fact );
  PC_TEST_CHECK( r->v_ == 123000142 && r->e_ == 4 );
  pd_sub( r, n1, n2, dec_fact );
  PC_TEST_CHECK( r->v_ == 122999857 && r->e_ == 4 );
  pd_sub( r, n2, n1, dec_fact );
  PC_TEST_CHECK( r->v_ == -122999857 && r->e_ == 4 );

  // test add/subtract odec_fact numbers with very large didec_factdec_fact. in magnitude
  pd_new( n1, 123, 27 );
  pd_new( n2, 1, 0 );
  pd_add( r, n1, n2, dec_fact );
  PC_TEST_CHECK( r->v_ == 123 && r->e_ == 27 );
  pd_add( r, n2, n1, dec_fact );
  PC_TEST_CHECK( r->v_ == 123 && r->e_ == 27 );
  pd_sub( r, n1, n2, dec_fact );
  PC_TEST_CHECK( r->v_ == 123 && r->e_ == 27 );
  pd_sub( r, n2, n1, dec_fact );
  PC_TEST_CHECK( r->v_ == -123 && r->e_ == 27 );

  pd_new( n1, 1010, -9 );
  pd_new( n2, 102, -8 );
  PC_TEST_CHECK( pd_lt( n1, n2, dec_fact ) );
  PC_TEST_CHECK( pd_gt( n2, n1, dec_fact ) );
}

void test_qs()
{
  PC_TEST_START
  test_pd();
  PC_TEST_END
}

int main( int argc,char** argv )
{
  if ( argc < 2 ) {
    return usage();
  }
  // unit test piece first
  test_qs();

  // construct quotes
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
  pc_price_t px[1];
  __builtin_memset( px, 0, sizeof( pc_price_t ) );
  uint64_t slot = 1000UL;
  px->last_slot_ = slot;
  px->agg_.pub_slot_ = slot;
  px->expo_      = pt.get_int( pt.find_val( 1, "exponent" ) );
  px->num_       = 0;
  uint32_t qt =pt.find_val( 1, "quotes" );
  for( uint32_t it = pt.get_first( qt ); it; it = pt.get_next( it ) ) {
    pc_price_comp_t *ptr = &px->comp_[px->num_++];
    ptr->latest_.status_ = PC_STATUS_TRADING;
    ptr->latest_.price_ = pt.get_int( pt.find_val( it,  "price"  ) );
    ptr->latest_.conf_  = pt.get_int( pt.find_val( it,  "conf"  ) );
    ptr->latest_.pub_slot_ = slot + pt.get_int( pt.find_val( it,  "slot_diff"  ) );
  }
  upd_aggregate( px, slot+1 );

  // generate result
  json_wtr jw;
  jw.add_val( json_wtr::e_obj );
  jw.add_key( "exponent", (int64_t)px->expo_ );
  jw.add_key( "price", px->agg_.price_ );
  jw.add_key( "conf", px->agg_.conf_ );
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
