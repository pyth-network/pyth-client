#include <stdio.h>
#include <math.h>
#include "prng.h"

#define EXP2M1_FXP_ORDER 7
#include "exp2m1.h"

static inline double exp2m1( double x ) { return expm1(0.69314718055994530943*x); } /* FIXME: USE PROPER METHOD */

static double const ulp_thresh[8] = {
         0.50000000000000000000, // For measuring limit
  92418389.07584851980209350586,
   4173744.68851661682128906250,
    183713.37186783552169799805,
      5998.71748709678649902344,
       165.76749026775360107422,
         4.96575236320495605469,
         0.63437640666961669922
};

static double const rel_thresh[8] = {
  4.7e-10, // For measuring limit
  6.2e-02,
  3.3e-03,
  1.3e-04,
  5.1e-06,
  1.5e-07,
  4.4e-09,
  5.5e-10
};

int
main( int     argc,
      char ** argv ) {
  (void)argc; (void)argv;

  prng_t _prng[1];
  prng_t * prng = prng_join( prng_new( _prng, (uint32_t)0, (uint64_t)0 ) );

  double const one_ulp = (double)(1<<30); /* exact */
  double const ulp     = (1./one_ulp);    /* exact */

  /* Exhaustively test x in [0,2^30) */

  double max_aerr = 0.;
  double max_rerr = 0.;

  double thresh = ulp_thresh[ EXP2M1_FXP_ORDER ];

  int ctr = 0;
  for( int i=0; i<(1<<30); i++ ) {
    if( !ctr ) { printf( "Completed %i iterations\n", i ); ctr = 100000000; }
    ctr--;

    double x = ulp*(double)i;                         /* exact */
    double y = ulp*(double)exp2m1_fxp( (uint64_t)i ); /* approx */
  //double y = ulp*round( one_ulp*exp2m1( x ) );      /* limit: bits 31.0 max_aerr 4.7e-10 max_rerr 4.7e-10 ulp 0.5 */
    double z = exp2m1( x );                           /* goal */
    double aerr = fabs(y-z);
    double rerr = aerr/(1.+z);
    if( aerr*one_ulp>thresh ) {
      printf( "FAIL (x %.20f y %.20f z %.20f aerr %.20f ulp)\n", x, y, z, aerr*one_ulp );
      return 1;
    }
    max_aerr = fmax( max_aerr, aerr );
    max_rerr = fmax( max_rerr, rerr );
  }

  /* Randomly test larger x (with coverage of each exponent range and
     special values) */

  thresh = rel_thresh[ EXP2M1_FXP_ORDER ];

  for( int i=0; i<10000000; i++ ) {
    uint64_t _x;
    if     ( i< 64 ) _x = UINT64_C(1)<<i;
    else if( i==64 ) _x = UINT64_C(0);
    else if( i==65 ) _x = exp2m1_fxp_max();
    else {
      int s  = ((int)prng_uint32( prng )) & 63;
      _x = prng_uint64( prng ) >> s;
    }
    uint64_t _y = exp2m1_fxp( _x );

    if( _x>exp2m1_fxp_max() ) {
      if( _y!=UINT64_MAX ) {
        printf( "FAIL (iter %i sat _x %lx y %lx)\n", i, _x, _y );
        return 1;
      }
      continue;
    }

    uint64_t xi = _x>>30;
    uint64_t xf = (_x<<34)>>34;
    if( !(xi & (xi-UINT64_C(1))) && !xf ) {
      if( _y!=((UINT64_C(1)<<(xi+UINT64_C(30)))-(UINT64_C(1)<<30)) ) {
        printf( "FAIL (iter %i exact_x %lx y %lx)\n", i, _x, _y );
        return 1;
      }
    }

    double x = ulp*(double)_x; /* exact */
    double y = ulp*(double)_y; /* exact */
    double z = exp2m1( x );    /* goal  */
    double aerr = fabs(y-z);
    double rerr = aerr/(1.+z);
    if( rerr>thresh ) {
      printf( "FAIL (x %.20f y %.20f z %.20f rerr %.20e)\n", x, y, z, rerr );
      return 1;
    }
  }

  prng_delete( prng_leave( prng ) );

  printf( "pass (bits %.1f max_aerr %.1e max_rerr %.1e ulp %.1f)\n", -log2( max_rerr ), max_aerr, max_rerr, max_aerr*one_ulp );

  return 0;
}

