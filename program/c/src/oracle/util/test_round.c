#include <stdio.h>

int
test_round() {
  unsigned i = (unsigned)0;

  int ctr = 0;
  for( int x=-32767; x<=32767; x++ ) {
    for( int y=-32767; y<=32767; y++ ) {
      if( !ctr ) { printf( "Completed %u iterations\n", i ); ctr = 10000000; }
      ctr--;

      int u = (x+y)>>1;
      int v = (x>>1)+(y>>1);
      int w = v + (x&y&1);
      if( u!=w ) {
        printf( "FAIL (x %3i y %3i u %3i v %3i w %3i)\n", x, y, u, v, w );
        return 1;
      }

      i++;
    }
  }
  printf( "pass\n" );

  return 0;
}
