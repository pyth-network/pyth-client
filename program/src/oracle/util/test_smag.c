#include <stdio.h>
#include "util.h"

int
main( int     argc,
      char ** argv ) {
  (void)argc; (void)argv;

  prng_t _prng[1];
  prng_t * prng = prng_join( prng_new( _prng, (uint32_t)0, (uint64_t)0 ) );

  int ctr = 0;                                                                                                                      
  for( int i=0; i<100000000; i++ ) {
    if( !ctr ) { printf( "Completed %i iterations\n", i ); ctr = 10000000; }
    ctr--;
#   define TEST(w) do {                                                      \
      int##w##_t x = (int##w##_t)prng_uint##w( prng );                       \
      int s; uint##w##_t m = smag_unpack_int##w( x, &s );                    \
      int##w##_t y = smag_pack_int##w( s, m );                               \
      uint##w##_t max = (uint##w##_t)(1ULL<<(w-1));                          \
      if( !(s==(x<(int##w##_t)0) && (m<max || (m==max && s==1))) ) {         \
        printf( "FAIL (iter %i op smag_unpack_int" #w " x %lx s %i m %lx\n", \
                i, (long unsigned)x, s, (long unsigned)m );                  \
        return 1;                                                            \
      }                                                                      \
      if( x!=y ) {                                                           \
        printf( "FAIL (iter %i op smag_pack_int" #w " s %i m %lx y %lx\n",   \
                i, s, (long unsigned)m, (long unsigned)y );                  \
        return 1;                                                            \
      }                                                                      \
      if( s!=signbit_int##w(x) ) {                                           \
        printf( "FAIL (iter %i op signbit_int" #w " x %lx s %i\n",           \
                i, (long unsigned)x, s );                                    \
        return 1;                                                            \
      }                                                                      \
      if( m!=abs_int##w(x) ) {                                               \
        printf( "FAIL (iter %i op abs_int" #w " x %lx m %lx\n",              \
                i, (long unsigned)x, (long unsigned)m );                     \
        return 1;                                                            \
      }                                                                      \
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

