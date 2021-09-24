#include "sort.h"

static inline void swap_int64( int64_t *a, int64_t *b )
{
  int64_t const temp = *a;
  *a = *b;
  *b = temp;
}

static inline int partition_int64( int64_t* v, int i, int j )
{
  int const p = i;
  int64_t const pv = v[ p ];

  while ( i < j ) {
    while ( v[ i ] <= pv && i <= j ) {
      ++i;
    }
    while ( v[ j ] > pv ) {
      --j;
    }
    if ( i < j ) {
      swap_int64( &v[ i ], &v[ j ] );
    }
  }
  swap_int64( &v[ p ], &v[ j ] );
  return j;
}

static void qsort_help_int64( int64_t* const v, int i, int j )
{
  if ( i >= j ) {
    return;
  }
  int p = partition_int64( v, i, j );
  qsort_help_int64( v, i, p - 1 );
  qsort_help_int64( v, p + 1, j );
}

void qsort_int64( int64_t* const v, unsigned const size )
{
  qsort_help_int64( v, 0, ( int )size - 1 );
}

