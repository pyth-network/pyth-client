char heap_start[8192];
#define PC_HEAP_START (heap_start)

#include <assert.h>
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
  const int64_t dec_fact[] = {
    1L, 10L, 100L, 1000L, 10000L, 100000L, 1000000L, 10000000L,
    100000000L, 1000000000L, 10000000000L, 100000000000L, 1000000000000L,
    10000000000000L, 100000000000000L, 1000000000000000L, 10000000000000000L,
    100000000000000000L
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

  // test add negative numbers
  pd_new( n1, -140205667, -13 );
  pd_new( n2, 42901738, -3 );
  pd_add( r, n1, n2, dec_fact );
  PC_TEST_CHECK( r->v_ == 42901737 && r->e_ == -3 );
  pd_add( r, n2, n1, dec_fact );
  PC_TEST_CHECK( r->v_ == 42901737 && r->e_ == -3 );

  pd_new( n1, 1010, -9 );
  pd_new( n2, 102, -8 );
  PC_TEST_CHECK( pd_lt( n1, n2, dec_fact ) );
  PC_TEST_CHECK( pd_gt( n2, n1, dec_fact ) );
}

int main( int argc,char** argv )
{
  PC_TEST_START
  test_pd();
  PC_TEST_END
  return 0;
}
