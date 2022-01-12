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
  for( int i=0; i<100000000; i++ ) {
    if( !ctr ) { printf( "Completed %i iterations\n", i ); ctr = 10000000; }
    ctr--;
#   define TEST(w) do {                                                                        \
      int         n    = ((int)prng_uint32( prng )) & (w-1);                                   \
      uint##w##_t mask = (uint##w##_t)(1ULL << n);                                             \
      uint##w##_t bit  = mask--;                                                               \
      uint##w##_t x    = bit | (prng_uint##w( prng ) & mask);                                  \
      int##w##_t  y    = (int##w##_t)x;                                                        \
      if( log2_uint##w( x )!=n ) {                                                             \
        printf( "FAIL (iter %i op log2_uint" #w " x %lx n %i\n", i, (long unsigned)x, n );     \
        return 1;                                                                              \
      }                                                                                        \
      if( y>0 && log2_int##w( y )!=n ) {                                                       \
        printf( "FAIL (iter %i op log2_int" #w " x %lx n %i\n",  i, (long unsigned)y, n );     \
        return 1;                                                                              \
      }                                                                                        \
      if( log2_uint##w##_def( x, 1234 )!=n ) {                                                 \
        printf( "FAIL (iter %i op log2_uint" #w "_def x %lx n %i\n", i, (long unsigned)x, n ); \
        return 1;                                                                              \
      }                                                                                        \
      if( log2_int##w##_def( y, 1234 )!=((y>(int##w##_t)0) ? n : 1234) ) {                     \
        printf( "FAIL (iter %i op log2_int" #w "_def y %lx n %i\n",  i, (long unsigned)x, n ); \
        return 1;                                                                              \
      }                                                                                        \
      n--; bit>>=1; mask>>=1; x>>=1; y=(int##w##_t)x;                                          \
      if( log2_uint##w##_def( x, 1234 )!=(x ? n : 1234) ) {                                    \
        printf( "FAIL (iter %i op log2_uint" #w "_def x %lx n %i\n", i, (long unsigned)x, n ); \
        return 1;                                                                              \
      }                                                                                        \
      if( log2_int##w##_def( y, 1234 )!=(y ? n : 1234) ) {                                     \
        printf( "FAIL (iter %i op log2_int" #w "_def x %lx n %i\n",  i, (long unsigned)x, n ); \
        return 1;                                                                              \
      }                                                                                        \
    } while(0)
    TEST(8);
    TEST(16);
    TEST(32);
    TEST(64);
#   undef TEST
  }

# undef N

  prng_delete( prng_leave( prng ) );

  printf( "pass\n" );

  return 0;
}

