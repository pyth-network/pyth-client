#include <stdio.h>
#include <math.h>
#include "prng.h"

#define REXP2_FXP_ORDER 7
#include "rexp2.h"

static double const ulp_thresh[8] = {
         0.50000000000000000000, // For measuring limit
  46209195.03792428970336914062,
   2086873.11166107654571533203,
     91856.43195831775665283203,
      3000.09640336036682128906,
        83.64191401004791259766,
         3.23314964771270751953,
         1.04669940471649169922
};

int
main( int     argc,
      char ** argv ) {
  (void)argc; (void)argv;

  prng_t _prng[1];
  prng_t * prng = prng_join( prng_new( _prng, (uint32_t)0, (uint64_t)0 ) );

  double const one_ulp = (double)(1<<30); /* exact */
  double const ulp     = (1./one_ulp);    /* exact */
  double const thresh  = ulp_thresh[ REXP2_FXP_ORDER ];

  double max_aerr = 0.;
  double max_rerr = 0.;

  /* Exhaustively test x in [0,2^30) */

  int ctr = 0;
  for( int i=0; i<(1<<30); i++ ) {
    if( !ctr ) { printf( "Completed %i iterations\n", i ); ctr = 100000000; }
    ctr--;

    double x = ulp*(double)i;                        /* exact */
    double y = ulp*(double)rexp2_fxp( (uint64_t)i ); /* approx */
  //double y = ulp*round( one_ulp*exp2( -x ) );      /* limit: bits 30.0 max_aerr 4.7e-10 max_rerr 9.3e-10 ulp 0.5 */
    double z = exp2( -x );                           /* goal */
    double aerr = fabs(y-z);
    double rerr = aerr/z;
    if( aerr*one_ulp>thresh ) {
      printf( "FAIL (x %.20f y %.20f z %.20f aerr %.20f ulp)\n", x, y, z, aerr*one_ulp );
      return 1;
    }
    max_aerr = fmax( max_aerr, aerr );
    max_rerr = fmax( max_rerr, rerr );
  }

  /* Randomly test larger x (with good coverage of each exponent range) */

  for( int i=0; i<10000000; i++ ) {
    int      s  = ((int)prng_uint32( prng )) & 63;
    uint64_t _x = prng_uint64( prng ) >> s;
    
    double x = ulp*(double)_x;              /* exact when _x is small enough to give a non-zero result */
    double y = ulp*(double)rexp2_fxp( _x ); /* approx */
    double z = exp2( -x );                  /* goal */
    double aerr = fabs(y-z);
    if( aerr*one_ulp>thresh ) {
      printf( "FAIL (x %.20f y %.20f z %.20f aerr %.20f ulp)\n", x, y, z, aerr*one_ulp );
      return 1;
    }
  }

  prng_delete( prng_leave( prng ) );

  printf( "pass (bits %.1f max_aerr %.1e max_rerr %.1e ulp %.1f)\n", -log2( max_rerr ), max_aerr, max_rerr, max_aerr*one_ulp );

  return 0;
}

