#include <stdio.h>
#include <math.h>
#include "../util/prng.h"

#define REXP2_FXP_ORDER 7 /* REXP2_FXP_ORDER 5 yields a better than IEEE single precision accurate approximation */
#include "overlap_model.h"

static long double
overlap_model_ref( uint64_t _mu_0, uint64_t _sigma_0,
                   uint64_t _mu_1, uint64_t _sigma_1 ) {
  /* Need long double because uint64_t not exactly representable in a
     IEEE double */
  static long double weight = ((long double)OVERLAP_MODEL_WEIGHT) / ((long double)(UINT64_C(1)<<30));
  if( !_sigma_0 && !_sigma_1 ) return _mu_0==_mu_1 ? weight : 0.L; 
  long double mu_0 = (long double)_mu_0; long double sigma_0 = (long double)_sigma_0;
  long double mu_1 = (long double)_mu_1; long double sigma_1 = (long double)_sigma_1;
  long double kappa_sq = 1.L/(sigma_0*sigma_0 + sigma_1*sigma_1);
  long double delta    = mu_0-mu_1;
  return weight*(sqrtl( (2.L*kappa_sq)*(sigma_0*sigma_1) )*expl( -(0.5L*kappa_sq)*(delta*delta) ));
}

static long double const ulp_fine[8] = {
         0.0L,
  46209190.1L, /* max_rerr 4.3e-02 */
   2086863.4L, /* max_rerr 1.9e-03 */
     91856.7L, /* max_rerr 8.6e-05 */
      2999.3L, /* max_rerr 2.8e-06 */
        83.2L, /* max_rerr 7.7e-08 */
         4.3L, /* max_rerr 4.0e-09 */
         3.6L  /* max_rerr 3.3e-09 */
};

static long double const ulp_coarse[8] = {
         0.0L,
  46209190.1L, /* max_rerr 4.3e-02 */
   2086863.4L, /* max_rerr 1.9e-03 */
     91856.7L, /* max_rerr 8.6e-05 */
     32768.0L, /* max_rerr 3.1e-05 */
     32768.0L, /* max_rerr 3.1e-05 */
     32768.0L, /* max_rerr 3.1e-05 */
     32768.0L  /* max_rerr 3.1e-05 */
};

int
main( int     argc,
      char ** argv ) {
  (void)argc; (void)argv;

  prng_t _prng[1];
  prng_t * prng = prng_join( prng_new( _prng, (uint32_t)0, (uint64_t)0 ) );

  long double max_ulp_fine   = 0.L;
  long double max_ulp_coarse = 0.L;

  long double thresh_fine   = ulp_fine  [ REXP2_FXP_ORDER ];
  long double thresh_coarse = ulp_coarse[ REXP2_FXP_ORDER ];

  int ctr = 0;
  for( int iter=0; iter<100000000; iter++ ) {
    if( !ctr ) { printf( "Completed %u iterations\n", iter ); ctr = 10000000; }
    ctr--;

    /* Generate random tests that really stress price invariance assumptions */

    uint32_t t       = prng_uint32( prng );
    uint64_t mu_0    = prng_uint64( prng ) >> (t & (uint32_t)63); t >>= 6;
    uint64_t mu_1    = prng_uint64( prng ) >> (t & (uint32_t)63); t >>= 6;
    uint64_t sigma_0 = prng_uint64( prng ) >> (t & (uint32_t)63); t >>= 6;
    uint64_t sigma_1 = prng_uint64( prng ) >> (t & (uint32_t)63); t >>= 6;

    /* When sigmas are really dissimilar, limitations of the fixed point
       representation limit accuracy to ~2^15 ulp.  So we do separate
       states for when the two distributions have similar widths. */

    uint64_t sigma_min = sigma_0<sigma_1 ? sigma_0 : sigma_1;
    uint64_t sigma_max = sigma_0<sigma_1 ? sigma_1 : sigma_0;
    int fine = (sigma_min >= (sigma_max>>1)); /* sigmas of the two are comparable */

    long double y   =           (long double)overlap_model    ( mu_0,sigma_0, mu_1,sigma_1 );
    long double z   = ((long double)(1<<30))*overlap_model_ref( mu_0,sigma_0, mu_1,sigma_1 );
    long double ulp = fabsl( y - z );
    if( ulp>=(fine ? thresh_fine : thresh_coarse) ) {
      printf( "FAIL (iter %i: i(%lu,%lu) j(%lu,%lu) y %Lf z %Lf ulp %Lf)\n", iter, mu_0, sigma_0, mu_1, sigma_1, y, z, ulp );
      return 1;
    }
    if( fine && ulp>max_ulp_fine   ) max_ulp_fine   = ulp;
    if(         ulp>max_ulp_coarse ) max_ulp_coarse = ulp;
  }

  prng_delete( prng_leave( prng ) );

  printf( "pass (fine: max_rerr %.1Le max ulp %.1Lf, coarse: max_rerr %.1Le max ulp %.1Lf)\n",
          max_ulp_fine   / ((long double)OVERLAP_MODEL_WEIGHT), max_ulp_fine,
          max_ulp_coarse / ((long double)OVERLAP_MODEL_WEIGHT), max_ulp_coarse );

  return 0;
}

