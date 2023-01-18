#define EXP2M1_FXP_ORDER 7
#define REXP2_FXP_ORDER 7
#include <stdio.h>
#include <math.h>
#include <tgmath.h>
#include <assert.h>
#include "../util/fxp.h"
#include "../util/exp2m1.h"
#include "../util/rexp2.h"
#include "../util/prng.h"


long double f1_ref( uint64_t gap ) {
	const long double gamma_l = exp( -1.0L / 9000.0L );
	return (1.0L - powl( gamma_l, gap ) )/(1.0L - gamma_l);
}
long double f1_ref2( uint64_t gap ) {
	const long double gamma_l = exp( -1.0L / 9000.0L );
	const long double log_gamma = -1.0L/9000.0L;
	return expm1l( log_gamma*gap ) / (gamma_l - 1.0L);
}
const uint64_t ONE_fxp = UINT64_C(1)<<30;
uint64_t f1_rexp( uint64_t gap ) {
	const uint64_t NEG_LOG_GAMMA  = UINT64_C(0xa8160e4168887aa3); /*2^76* -lg(gamma). Takes up 64 bits */
	const uint64_t F1_DENOMINATOR = UINT64_C(0x8ca200026d60dcdf); /*2^50/(1-gamma). Takes up the full 64 bits, which is fine */
	uint64_t carry  = 0;
	uint64_t numerator = fxp_sub_fast( ONE_fxp, rexp2_fxp( (fxp_mul_rna( gap, NEG_LOG_GAMMA, &carry ) + (1<<15) )>>16 ) );
	assert( carry == 0 );
	uint64_t result = (fxp_mul_rne( numerator, F1_DENOMINATOR, &carry )+(1<<19))>>20;
	assert( carry == 0 );
	return result;
}
uint64_t f1_exp2m1( uint64_t gap ) {
	/* numerator = 1-exp2m1(1- ell*gap) */
	const uint64_t NEG_LOG_GAMMA  = UINT64_C(0xa8160e4168887aa3); /*2^76* -lg(gamma). Takes up 64 bits */
	uint64_t carry  = 0;
	uint64_t numerator = fxp_sub_fast( ONE_fxp, exp2m1_fxp( (fxp_sub_fast( (ONE_fxp<<33)+(1ULL<<32), fxp_mul_rna( gap<<17, NEG_LOG_GAMMA, &carry ) )>>33) ) );
	/*																																														  = 2^63*(ell*gap) */
	assert( carry == 0 );
	const uint64_t F1_DENOMINATOR = UINT64_C(0x8ca200026d60dcdf); /*2^50/(1-gamma). Takes up the full 64 bits, which is fine */
	uint64_t result = (fxp_mul_rne( numerator, F1_DENOMINATOR, &carry )+(1<<20))>>21;
	assert( carry == 0 );
	return result;
}
uint64_t f1_impl3( uint64_t gap ) {
	const uint64_t NEG_LOG_GAMMA      = UINT64_C(0xa8160e4168887aa3); /*2^76* -lg(gamma). Takes up 64 bits */
	const uint64_t F1_CONST = UINT64_C(0x8ca200022d60dcdf); /*2^20/(1-gamma)-1 as an fxp, i.e. 2^50/(1-gamma)-2^30. Takes up the full 64 bits, which is fine */
	const uint64_t EXP_BIAS = UINT64_C(0x848b0c62c35c228f); /* 2**58*(20+r)+(1<<27) where r=lg(1/(1-gamma)) */
	uint64_t carry  = 0;
	uint64_t exponential =  exp2m1_fxp( fxp_sub_fast( EXP_BIAS, fxp_mul_rna( gap<<12, NEG_LOG_GAMMA, &carry ) )>>28 ) ;
	assert( carry == 0 );
	return fxp_sub_fast( F1_CONST+(1<<19), exponential )>>20;
}
uint64_t f1_impl4( uint64_t gap ) {
	const uint64_t NEG_LOG_GAMMA      = UINT64_C(0xa8160e4168887aa3); /*2^76* -lg(gamma). Takes up 64 bits */
	const uint64_t F1_CONST = UINT64_C(0x8ca200026d86d800); /*2^20/(1-gamma) as an fxp, i.e. 2^50/(1-gamma). Takes up the full 64 bits, which is fine */
	const uint64_t EXP_BIAS = UINT64_C(0x848b0c62c35c228f); /* 2**58*(20+r)+(1<<27) where r=lg(1/(1-gamma)) */
	uint64_t carry  = 0;
	uint64_t exponential =  fxp_exp2_approx( fxp_sub_fast( EXP_BIAS, fxp_mul_rna( gap<<12, NEG_LOG_GAMMA, &carry ) )>>28 ) ;
	assert( carry == 0 );
	return fxp_sub_fast( F1_CONST+(1<<19), exponential )>>20;
}
uint64_t f1_ref2fxp( uint64_t gap ) {
	return (uint64_t) ( (long double)ONE_fxp * f1_ref2( gap ) + 0.5L );
}

long double f2_ref( uint64_t gap ) {
	const long double gamma_l = exp( -1.0L / 9000.0L );
	return (1.0L - 2.0L * powl( gamma_l, gap ) )/(1.0L - gamma_l * powl( 2.0L, 1.0L/gap ) );
}
long double f2_approx_ref( uint64_t gap ) {
	const long double gamma_l = exp( -1.0L / 9000.0L );
	const long double ell_l = ( 1.0L / 9000.0L )*log2l( expl( 1.0L ) );
	const long double log2_l = logl( 2.0L );
	return (2.0L * powl( gamma_l, gap )  - 1.0L )*(2*gap-1) / (2*log2_l - gap*ell_l*2.0L*log2_l);
}
uint64_t f2_approxref_fxp( uint64_t gap ) {
	return (uint64_t) ( (long double)ONE_fxp * f2_approx_ref( gap ) + 0.5L );
}

uint64_t f2_exp2m1( uint64_t gap ) {
	const uint64_t NEG_LOG_GAMMA      = UINT64_C(0xa8160e4168887aa3); /*2^76* -lg(gamma). Takes up 64 bits */
	uint64_t carry = 0;
	uint64_t numerator = exp2m1_fxp( ( (ONE_fxp<<33) + (1ULL<<32) - fxp_mul_rna( gap<<17, NEG_LOG_GAMMA, &carry ))>>33 );
	uint64_t denominator = exp2m1_fxp( (((ONE_fxp<<33)+(gap>>1))/gap + (1ULL<<32) - ((NEG_LOG_GAMMA + (1<<12))>>13))>>33 );
	/* numerator and denominator in [0,1) as fixed point numbers */
	uint64_t result = fxp_div_rna( numerator, denominator, &carry );
	assert( carry == 0 );
	return result;
}

uint64_t f2_lhapprox( uint64_t gap ) {
	//if( gap<5000 ) return f2_exp2m1( gap );

	const uint64_t NEG_LOG_GAMMA      = UINT64_C(0xa8160e4168887aa3); /*2^76* -lg(gamma). Takes up 64 bits */
	const uint64_t TWO_LOG_2          = UINT64_C(0xb17217f7d1cf79ac); /*2^63*(2*log(2)). (that's base e log for once). Takes up 64 bits */
	const uint64_t TWO_L_LOG_2        = UINT64_C(0xe90452d489718cdb); /*2^76*(2*log(2)* -lg(gamma)). Takes up 64 bits */
	uint64_t carry = 0;
	uint64_t a_numerator = exp2m1_fxp( ( (ONE_fxp<<33) + (1ULL<<32) - fxp_mul_rna( gap<<17, NEG_LOG_GAMMA, &carry ))>>33 ); /* 2^30 exp2m1(). <2^30 */
	assert( carry == 0 );
	uint64_t b_numerator = (2*gap - 1); /* Scale factor of 2^0 */
	uint64_t numerator = a_numerator * b_numerator;  /* Scale factor of 2^30, <2^44 */
	uint64_t b_denominator = (TWO_LOG_2 - fxp_mul_rna( gap<<17, TWO_L_LOG_2, &carry ) ); /* has 2^63 scale factor */
	assert( carry == 0 );

	uint64_t result = fxp_div_rna( numerator<<20, (b_denominator+(1<<12))>>13, &carry ); /* Do the division with a common scale factor of 2^50 */
	assert( carry == 0 );
	return result;
}
uint64_t f2_padeapprox( uint64_t gap ) { 
  /* Valid for 4000-6000 */
	if( gap<4000 || gap>6000 ) return f2_exp2m1( gap );

}

uint64_t f2_cubicapprox( uint64_t gap ) {
	//if( gap<4000 ) return (uint64_t)((1UL<<30)*f2_ref( gap ));
	if( gap<2500 ) return f2_exp2m1( gap );
  uint64_t shifted_x;
  uint64_t A; 
  uint64_t MB;
  uint64_t C ;
  uint64_t D ;
  if( gap<3200 ){
    shifted_x = gap-2500;
    A  = UINT64_C(       0x2b38eac74); /*  2^62*a */
    MB = UINT64_C(    0x11714c3f6066); /* -2^58*b */
    C  = UINT64_C(  0x4390a8bca989c0); /* 2^54*c */
    D  = UINT64_C(0x60d7c9e48a30f800); /* 2^51*d */
  } else if( gap<4000 ) {
    shifted_x = gap-3200;
    A  = UINT64_C(       0x27a51927c); /*  2^62*a */
    MB = UINT64_C(    0x100e0de3186a); /* -2^58*b */
    C  = UINT64_C(  0x3dd6c06b7f49cc); /* 2^54*c */
    D  = UINT64_C(0x76f1ced50eae8400); /* 2^51*d */
  } else if( gap<5000 ) {
    shifted_x = gap-4000;
    A  = UINT64_C(       0x23bcc447b); /*  2^62*a */
    MB = UINT64_C(     0xe98c17b3db6); /* -2^58*b */
    C  = UINT64_C(  0x37d9697300c306); /* 2^54*c */
    D  = UINT64_C(0x8de99434cf868000); /* 2^51*d */
  } else if( gap<6000 ) {
    shifted_x = gap-5000;
    A  = UINT64_C(       0x1fd687471); /*  2^62*a */
    MB = UINT64_C(     0xcf65f27a5ed); /* -2^58*b */
    C  = UINT64_C(  0x311e89c5642fc2); /* 2^54*c */
    D  = UINT64_C(0xa781ec045c2fa000); /* 2^51*d */
  } else {
    shifted_x = gap-6000;
    A  = UINT64_C(       0x1d300d4ef); /*  2^62*a */
    MB = UINT64_C(     0xb84c170acf6); /* -2^58*b */
    C  = UINT64_C(  0x2b250d2f160960); /* 2^54*c */
    D  = UINT64_C(0xbe01128a68e27000); /* 2^51*d */
  }
  uint64_t ax3 = A*shifted_x*shifted_x*shifted_x; /* < 2^63.1, scaled by 2^62 */
  uint64_t bx2 = MB*shifted_x*shifted_x; /* < 2^63.8, scaled by -2^58 */
  uint64_t cx  = C*shifted_x; /* < 2^63.8, scaled by 2^54 */
  uint64_t part1 = (cx - ((bx2+(1<<3))>>4)) + ((ax3 + (1<<7))>>8); /* Scaled by 2^54 */
  uint64_t part2 = ((part1 + (1<<2))>>3) + D; /* Scaled by 2^51 */
  return (part2 + (1<<20))>>21;
}
long double f0_ref( uint64_t gap ) {
	const long double gamma_l = exp( -1.0L / 9000.0L );
  return powl( gamma_l, gap );
}
uint64_t f0_rexp( uint64_t gap ) {
	const uint64_t NEG_LOG_GAMMA  = UINT64_C(0xa8160e4168887aa3); /*2^76* -lg(gamma). Takes up 64 bits */
	uint64_t carry  = 0;
  uint64_t result = rexp2_fxp( (fxp_mul_rna( gap, NEG_LOG_GAMMA, &carry ) + (1<<15))>>16 );
  assert( carry == 0 );
  return result;
}

int
main( int     argc,
      char ** argv ) {
  (void)argc; (void)argv;

  prng_t _prng[1];
  prng_t * prng = prng_join( prng_new( _prng, (uint32_t)0, (uint64_t)0 ) );


#define TEST_IMPL(name, compare, debug) do{ 							                    \
														long double max_ulp = 0.L; \
														long double max_relerr = 0.L; \
														for( uint64_t gap=1ULL; gap<6238ULL; gap++ ) { \
															long double impl = (long double)name(   gap ); \
															long double exact = ((long double)(1<<30))*compare( gap ); \
															long double ulp = fabsl( impl - exact ); \
															int new_worst = (ulp>max_ulp)||(ulp/exact>max_relerr); \
															if( ulp>max_ulp ) max_ulp = ulp; \
															if( ulp/exact>max_relerr ) max_relerr = ulp/exact; \
														  if( (debug) && new_worst) { printf( "%ld: max ulp for " #name ": %.2Lf. max rel err: %Lg\n", gap, max_ulp, max_relerr );  } \
														} \
														printf( "max ulp for " #name ": %.2Lf. max rel err: %Lg\n", max_ulp, max_relerr ); \
													} while(0)
  TEST_IMPL(f0_rexp,    f0_ref,  0 );

	TEST_IMPL(f1_rexp,    f1_ref2, 0 );
	TEST_IMPL(f1_exp2m1,  f1_ref2, 0 );
	TEST_IMPL(f1_impl3,   f1_ref2, 0 );
	TEST_IMPL(f1_impl4,   f1_ref2, 0 );
	TEST_IMPL(f1_ref2fxp, f1_ref,  0 );

	TEST_IMPL( f2_exp2m1, f2_ref, 0 );
	TEST_IMPL( f2_lhapprox, f2_ref, 0 );
	TEST_IMPL( f2_cubicapprox, f2_ref, 0 );
	{
    printf("gap exp2m1 lhopital_approx cubic_approx\n");
		for( uint64_t gap=1ULL; gap<6238ULL; gap+=5 ) { 
			long double expval = (long double)f2_exp2m1(   gap ); 
			long double lhval = (long double)f2_lhapprox(   gap ); 
			long double cubicval = (long double)f2_cubicapprox(   gap ); 
			long double ld_exact = ((long double)(1<<30))*f2_ref(   gap ); 
			printf("%lu  %Lf %Lf %Lf\n", gap, fabsl( expval-ld_exact ), fabsl( lhval-ld_exact ), fabsl( cubicval-ld_exact ) );
		}
	}
	printf("\n\n");
	TEST_IMPL( f2_lhapprox, f2_approx_ref, 0 );
	TEST_IMPL( f2_approxref_fxp, f2_ref, 0 );
	printf("\n\n");
	{
    long double worst = 0.0L;
    printf("gap eps/P\n");
		for( uint64_t gap=1ULL; gap<6238ULL; gap+=1 ) { 
      // c1 = f0. c2 = 2f1-f2, c3=f2-f1
      long double c1_ref   = f0_ref( gap );
      long double c1eps1   = fabsl( (1UL<<30)*f0_ref( gap ) - f0_rexp( gap ) );

      long double c2_ref   = 2.0L * f1_ref( gap ) - f2_ref( gap );
      long double c2_fxp   = 2L* f1_impl4( gap ) > f2_cubicapprox( gap ) ? 2L* f1_impl4( gap ) - f2_cubicapprox( gap ) : 0;
      long double c2eps2   = fabsl( (1UL<<30)*c2_ref - c2_fxp );

      long double c3_ref   = f2_ref( gap ) - f1_ref( gap );
      long double c3_fxp   = f2_cubicapprox( gap ) > f1_impl4( gap ) ? f2_cubicapprox( gap ) - f1_impl4( gap ) : 0;
      long double c3eps3   = fabsl( (1UL<<30)*c3_ref - c3_fxp );

      long double scalar = 1<<30;
      long double numerator   = (c1eps1/c1_ref) + c2eps2 + c3eps3; // scaled by 2^30
      long double denominator = 1.0L - c1_ref - (c1eps1/scalar);

      long double quotient = (numerator/denominator)/scalar;
      if( quotient>worst ) worst=quotient;
      printf( "%ld %Lf %Lf %Lf %Lf\n", gap, (c1eps1/c1_ref)/scalar, c2eps2/scalar, c3eps3/scalar, 1.0L/denominator );
		}
    printf("Worst: %Lf\n", worst);
	}


  prng_delete( prng_leave( prng ) );


  return 0;
}

