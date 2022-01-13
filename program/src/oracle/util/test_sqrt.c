#include <stdio.h>
#include "util.h"

int
main( int     argc,
      char ** argv ) {
  (void)argc; (void)argv;

  prng_t _prng[1];
  prng_t * prng = prng_join( prng_new( _prng, (uint32_t)0, (uint64_t)0 ) );

  int ctr = 0;                                                                                                                      
  for( int i=0; i<50000000; i++ ) {
    if( !ctr ) { printf( "Completed %i iterations\n", i ); ctr = 1000000; }
    ctr--;

#   define TEST(w) do {                                                     \
      int         n = ((int)prng_uint32( prng )) & (w-1);                   \
      uint##w##_t x = (uint##w##_t)(prng_uint##w( prng ) >> n);             \
      uint##w##_t y = sqrt_uint##w( x );                                    \
      uint##w##_t r = (uint##w##_t)(x - y*y);                               \
      if( y>=(UINT##w##_C(1)<<(w/2)) || r>(y<<1) ) {                        \
        printf( "FAIL (iter %i op sqrt_uint" #w " x %lx y %lx r %lx)\n",    \
                i, (long unsigned)x, (long unsigned)y, (long unsigned)r );  \
        return 1;                                                           \
      }                                                                     \
      int##w##_t u = (int##w##_t)x;                                         \
      if( u>=INT##w##_C(0) ) {                                              \
        int##w##_t v = sqrt_int##w( u );                                    \
        int##w##_t s = (int##w##_t)(u - v*v);                               \
        if( !(INT##w##_C(0)<=s && s<=(v<<1)) ) {                            \
          printf( "FAIL (iter %i op sqrt_int" #w " u %li v %li s %li)\n",   \
                  i, (long)u, (long)v, (long)s );                           \
          return 1;                                                         \
        }                                                                   \
      }                                                                     \
      int##w##_t v = sqrt_re_int##w( u );                                   \
      int##w##_t s = (int##w##_t)(u - v*v);                                 \
      if( !( (u>=INT##w##_C(0) && INT##w##_C(0)<=s && s<=(v<<1)) ||         \
             (u< INT##w##_C(0) && v==INT##w##_C(0)             ) ) ) {      \
        printf( "FAIL (iter %i op sqrt_re_int" #w " u %li v %li s %li)\n",  \
                i, (long)u, (long)v, (long)s );                             \
        return 1;                                                           \
      }                                                                     \
      v = sqrt_abs_int##w( u );                                             \
      s = (int##w##_t)sqrt_uint##w( abs_int##w( u ) );                      \
      if( v!=s ) {                                                          \
        printf( "FAIL (iter %i op sqrt_abs_int" #w " u %li v %li s %li)\n", \
                i, (long)u, (long)v, (long)s );                             \
        return 1;                                                           \
      }                                                                     \
    } while(0)
    TEST(8);
    TEST(16);
    TEST(32);
    TEST(64);
#   undef TEST

  }

  prng_delete( prng_leave( prng ) );

  printf( "pass\n" );

  return 0;
}

