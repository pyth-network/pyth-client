#include <stdio.h>
#include <limits.h>
#include "util.h"

int
main( int     argc,
      char ** argv ) {
  (void)argc; (void)argv;

  uint32_t shift_mask = (uint32_t)(sizeof(uintptr_t)*(size_t)CHAR_BIT);
  if( !align_ispow2( (uintptr_t)shift_mask ) ) {
    printf( "FAIL (platform)\n" );
    return 1;
  }
  shift_mask--;

  prng_t _prng[1];
  prng_t * prng = prng_join( prng_new( _prng, (uint32_t)0, (uint64_t)0 ) );

  int ctr = 0;
  for( int i=0; i<1000000000; i++ ) {
    if( !ctr ) { printf( "Completed %i iterations\n", i ); ctr = 10000000; }
    ctr--;

    /* Test align_ispow2 */

    do {
      uint32_t r = prng_uint32( prng );
      uint32_t s = (r>>8) & shift_mask;  /* s is uniform IID valid right shift amount for a uintptr_t */
      r &= (uint32_t)255;                /* r is 8-bit uniform IID rand (power of 2 with prob ~8/256=1/32) */
      uintptr_t x = ((uintptr_t)r) << s; /* x is power of 2 with prob ~1/32 */

      int c = align_ispow2( x );
      int d = (x>(uintptr_t)0) && !(x & (x-(uintptr_t)1));
      if( c!=d ) {
        printf( "FAIL (x %lx c %i d %i)\n", (unsigned long)x, c, d );
        return 1;
      }
    } while(0);

    /* Test align_up/align_dn/align_isaligned */

    do {
      uintptr_t a = ((uintptr_t)1) << (prng_uint32( prng ) & shift_mask);

      uintptr_t x = (uintptr_t)prng_uint64( prng );
      void *    u = (void *)x;
      void *    v = align_dn( u, a );
      void *    w = align_up( u, a );
      uintptr_t y = (uintptr_t)v;
      uintptr_t z = (uintptr_t)w;

      int already_aligned   = (u==v);
      int already_aligned_1 = (u==w);

      uintptr_t mask = a-(uintptr_t)1;
      if( already_aligned != already_aligned_1     ||
          align_isaligned( u, a )!=already_aligned ||
          !align_isaligned( v, a )                 ||
          !align_isaligned( w, a )                 ||
          ( x       & ~mask) != y                  ||
          ((x+mask) & ~mask) != z                  ) {
        printf( "FAIL (a %lx aa %i aa1 %i x %lx y %lx z %lx)\n",
                (unsigned long)a, already_aligned, already_aligned_1, (unsigned long)x, (unsigned long)y, (unsigned long)z );
        return 1;
      }

    } while(0);
  }

  prng_delete( prng_leave( prng ) );

  printf( "pass\n" );

  return 0;
}
