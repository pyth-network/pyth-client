#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../util/util.h"
#include "model.h"

int
qcmp( void const * _p,
      void const * _q ) {
  int64_t p = *(int64_t const *)_p;
  int64_t q = *(int64_t const *)_q;
  if( p < q ) return -1;
  if( p > q ) return  1;
  return 0;
}

int
main( int     argc,
      char ** argv ) {
  (void)argc; (void)argv;

  prng_t _prng[1];
  prng_t * prng = prng_join( prng_new( _prng, (uint32_t)0, (uint64_t)0 ) );

# define N 96

  int64_t quote0 [N];
  int64_t quote  [N];
  int64_t val    [3];
  int64_t scratch[N];

  int ctr = 0;
  for( int iter=0; iter<10000000; iter++ ) {
    if( !ctr ) { printf( "Completed %u iterations\n", iter ); ctr = 100000; }
    ctr--;

    /* Generate a random test */

    uint64_t cnt = (uint64_t)1 + (uint64_t)(prng_uint32( prng ) % (uint32_t)N); /* In [1,N], approx uniform IID */
    for( uint64_t idx=(uint64_t)0; idx<cnt; idx++ ) quote0[ idx ] = (int64_t)prng_uint64( prng );

    /* Apply the model */

    memcpy( quote, quote0, sizeof(int64_t)*(size_t)cnt );
    if( price_model( cnt, quote, val+0, val+1, val+2, scratch )!=quote ) { printf( "FAIL (compose)\n" ); return 1; }

    /* Validate the results */

    qsort( quote0, (size_t)cnt, sizeof(int64_t), qcmp );
    if( memcmp( quote, quote0, sizeof(int64_t)*(size_t)cnt ) ) { printf( "FAIL (sort)\n" ); return 1; }

    uint64_t p25_idx = cnt>>2;
    uint64_t p50_idx = cnt>>1;
    uint64_t p75_idx = cnt - (uint64_t)1 - p25_idx;
    uint64_t is_even = (uint64_t)!(cnt & (uint64_t)1);

    if( val[0]!=quote[ p25_idx ] ) { printf( "FAIL (p25)\n" ); return 1; }
    if( val[1]!=avg_2_int64( quote[ p50_idx-is_even ], quote[ p50_idx ] ) ) { printf( "FAIL (p50)\n" ); return 1; }
    if( val[2]!=quote[ p75_idx ] ) { printf( "FAIL (p75)\n" ); return 1; }
  }

# undef N

  prng_delete( prng_leave( prng ) );

  printf( "pass\n" );
  return 0;
}

