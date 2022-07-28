#include <stdio.h>
#include "util.h"

int
main( int     argc,
      char ** argv ) {
  (void)argc; (void)argv;

  prng_t _prng[1];
  prng_t * prng = prng_join( prng_new( _prng, (uint32_t)0, (uint64_t)0 ) );

  int ctr;

  ctr = 0;
  for( int i=0; i<1000000000; i++ ) {
    if( !ctr ) { printf( "reg: Completed %i iterations\n", i ); ctr = 10000000; }
    ctr--;

#   define TEST(w) do {                                                                      \
      uint##w##_t x = prng_uint##w( prng );                                                  \
      uint##w##_t y = prng_uint##w( prng );                                                  \
      uint##w##_t z = avg_2_uint##w( x, y );                                                 \
      uint##w##_t r = (uint##w##_t)(((uint64_t)x+(uint64_t)y) >> 1);                         \
      if( z!=r ) {                                                                           \
        printf( "FAIL (iter %i op avg_2_uint" #w "x %lu y %lu z %lu r %lu\n",                \
                i, (long unsigned)x, (long unsigned)y, (long unsigned)z, (long unsigned)r ); \
        return 1;                                                                            \
      }                                                                                      \
    } while(0)
    TEST(8);
    TEST(16);
    TEST(32);
#   undef TEST

    do {
      uint64_t x = prng_uint64( prng );
      uint64_t y = prng_uint64( prng );
      uint64_t z = avg_2_uint64( x, y );
      uint64_t r = (x>>1) + (y>>1) + (x & y & (uint64_t)1);
      if( z!=r ) {
        printf( "FAIL (iter %i op avg_2_uint64 x %lu y %lu z %lu r %lu\n",
                i, (long unsigned)x, (long unsigned)y, (long unsigned)z, (long unsigned)r );
        return 1;
      }
    } while(0);
                                                                                             \
#   define TEST(w) do {                                                                      \
      int##w##_t x = (int##w##_t)prng_uint##w( prng );                                       \
      int##w##_t y = (int##w##_t)prng_uint##w( prng );                                       \
      int##w##_t z = avg_2_int##w( x, y );                                                   \
      int##w##_t r = (int##w##_t)(((int64_t)x+(int64_t)y) >> 1);                             \
      if( z!=r ) {                                                                           \
        printf( "FAIL (iter %i op avg_2_int" #w "x %li y %li z %li r %li\n",                 \
                i, (long)x, (long)y, (long)z, (long)r );                                     \
        return 1;                                                                            \
      }                                                                                      \
    } while(0)
    TEST(8);
    TEST(16);
    TEST(32);
#   undef TEST

    do {
      int64_t x = (int64_t)prng_uint64( prng );
      int64_t y = (int64_t)prng_uint64( prng );
      int64_t z = avg_2_int64( x, y );
      int64_t r = (x>>1) + (y>>1) + (x & y & (int64_t)1);
      if( z!=r ) {
        printf( "FAIL (iter %i op avg_2_int64 x %li y %li z %li r %li\n",
                i, (long)x, (long)y, (long)z, (long)r );
        return 1;
      }
    } while(0);
  }

# define N 512

  ctr = 0;
  for( int i=0; i<10000000; i++ ) {
    if( !ctr ) { printf( "mem: Completed %i iterations\n", i ); ctr = 100000; }
    ctr--;

#   define TEST(w) do {                                                    \
      uint##w##_t x[N];                                                    \
      uint32_t n = prng_uint32( prng ) & (uint32_t)(N-1);                  \
      uint64_t a = (uint64_t)0;                                            \
      for( uint32_t i=(uint32_t)0; i<n; i++ ) {                            \
        uint##w##_t xi = prng_uint##w( prng );                             \
        x[i] = xi;                                                         \
        a += (uint64_t)xi;                                                 \
      }                                                                    \
      if( n ) a /= (uint64_t)n;                                            \
      uint##w##_t z = avg_uint##w( x, n );                                 \
      uint##w##_t r = (uint##w##_t)a;                                      \
      if( z!=r ) {                                                         \
        printf( "FAIL (iter %i op avg_uint" #w " n %lu z %lu r %lu\n",     \
                i, (long unsigned)n, (long unsigned)z, (long unsigned)r ); \
        return 1;                                                          \
      }                                                                    \
    } while(0)

    TEST(8);
    TEST(16);
    TEST(32);

#   undef TEST

#   define TEST(w) do {                                                               \
      int##w##_t x[N];                                                                \
      uint32_t n = prng_uint32( prng ) & (uint32_t)(N-1);                             \
      int64_t a = (int64_t)0;                                                         \
      for( uint32_t i=(uint32_t)0; i<n; i++ ) {                                       \
        int##w##_t xi = (int##w##_t)prng_uint##w( prng );                             \
        x[i] = xi;                                                                    \
        a += (int64_t)xi;                                                             \
      }                                                                               \
      if( n ) a /= (int64_t)n; /* Assumes round to zero signed int div on platform */ \
      int##w##_t z = avg_int##w( x, n );                                              \
      int##w##_t r = (int##w##_t)a;                                                   \
      if( z!=r ) {                                                                    \
        printf( "FAIL (iter %i op avg_int" #w " n %lu z %li r %li\n",                 \
                i, (long unsigned)n, (long)z, (long)r );                              \
        return 1;                                                                     \
      }                                                                               \
    } while(0)

    TEST(8);
    TEST(16);
    TEST(32);

#   undef TEST

  }

# undef N

  prng_delete( prng_leave( prng ) );

  printf( "pass\n" );

  return 0;
}
