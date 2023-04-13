#include <stdio.h>
#include "../util/util.h"

#define BEFORE(i,j) (((i)>>16)<((j)>>16))

#define SORT_NAME        sort
#define SORT_KEY_T       int
#define SORT_IDX_T       int
#define SORT_BEFORE(i,j) BEFORE(i,j)
#include "tmpl/sort_stable.c"

int test_sort_stable() {

# define N 96
  int x[N];
  int y[N];
  int w[N];

  /* Brute force validate small sizes via the 0-1 principle (with
     additional information in the keys to validate stability as well). */

  for( int n=0; n<=24; n++ ) {
    for( long b=0L; b<(1L<<n); b++ ) {
      for( int i=0; i<n; i++ ) x[i] = (((int)((b>>i) & 1L))<<16) | i;
      for( int i=0; i<n; i++ ) w[i] = x[i];

      int * z = sort_stable( x,n, y );

      /* Make sure that z is a permutation of input data */
      for( int i=0; i<n; i++ ) {
        int j = z[i] & (int)0xffff; /* j is the index where z was initially */
        if( j<0 || j>=n || z[i]!=w[j] ) { printf( "FAIL (corrupt)\n" ); return 1; }
        w[j] = -1; /* Mark that this entry has already been confirmed */
      }
      for( int i=0; i<n; i++ ) if( w[i]!=-1 ) { printf( "FAIL (perm)\n" ); return 1; }

      /* Make sure that z is in order and stable */
      for( int i=1; i<n; i++ )
        if( z[i]<=z[i-1] ) { printf( "FAIL (%s, b=%lx)\n", BEFORE( z[i], z[i-1] ) ? "order" : "stable", b ); return 1; }
    }
  }

  /* Randomized validation for larger sizes */

  prng_t _prng[1];
  prng_t * prng = prng_join( prng_new( _prng, (uint32_t)0, (uint64_t)0 ) );

  int ctr = 0;
  for( int iter=0; iter<10000000; iter++ ) {
    if( !ctr ) { ctr = 100000; }
    ctr--;

    int n = (int)(prng_uint32( prng ) % (uint32_t)(N+1)); /* In [0,N], approx uniform IID */
    for( int i=0; i<n; i++ ) x[i] = (int)((prng_uint32( prng ) & UINT32_C( 0x00ff0000 )) | (uint32_t)i);
    for( int i=0; i<n; i++ ) w[i] = x[i];

    int * z = sort_stable( x,n, y );

    /* Make sure that z is a permutation of input data */
    for( int i=0; i<n; i++ ) {
      int j = z[i] & (int)0xffff; /* j is the index where z was initially */
      if( j<0 || j>=n || z[i]!=w[j] ) { printf( "FAIL (corrupt)\n" ); return 1; }
      w[j] = -1; /* Mark that this entry has already been confirmed */
    }
    for( int i=0; i<n; i++ ) if( w[i]!=-1 ) { printf( "FAIL (perm)\n" ); return 1; }

    /* Make sure that z is in order and stable */
    for( int i=1; i<n; i++ )
      if( z[i]<=z[i-1] ) { printf( "FAIL (%s)\n", BEFORE( z[i], z[i-1] ) ? "order" : "stable" ); return 1; }
  }

  return 0;
}

#undef BEFORE
