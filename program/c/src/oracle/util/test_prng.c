#include <stdio.h>
#include <limits.h>
#include <math.h>
#include "util.h"

/* First 10 uint64 for seq 0 starting from idx 0 */

uint64_t x0_0[10] = {
  UINT64_C( 0xa036f9b67313c1aa ),
  UINT64_C( 0x110a4ea5e65927d2 ),
  UINT64_C( 0x9ddf34cf83d17c94 ),
  UINT64_C( 0xeb33d1b534e210ec ),
  UINT64_C( 0x9b15b94d3e81b76a ),
  UINT64_C( 0xee9544dab2bf64bf ),
  UINT64_C( 0x5c4b0ccf7c94d274 ),
  UINT64_C( 0xf0f83ab44262ad1f ),
  UINT64_C( 0xf11b1aa14c7dabd6 ),
  UINT64_C( 0x3800dde6d02d6ed7 )
};

/* First 10 uint64 for seq 0 starting from idx 20 */

uint64_t x1_20[10] = {
  UINT64_C( 0x55369a6a0817cbce ),
  UINT64_C( 0xce1baeb695229132 ),
  UINT64_C( 0x0e443e81e9e722d2 ),
  UINT64_C( 0xc6c065484f76e825 ),
  UINT64_C( 0x37dc474806fabc8a ),
  UINT64_C( 0x9fa3305df1b56824 ),
  UINT64_C( 0xf3b3961a17ed881c ),
  UINT64_C( 0x646f40006cef8d6f ),
  UINT64_C( 0x4d6955f607a153b2 ),
  UINT64_C( 0xf806bccb58d0e60b )
};

void
printf_ref( uint64_t * ref,
            uint32_t   seq,
            uint64_t   idx ) {
  printf( "uint64_t x%u_%lu[10] = {\n", seq, idx );
  for( int i=0; i<10; i++ ) printf( "  UINT64_C( 0x%016lx )%s\n", ref[i], i<9 ? "," : "" );
  printf( "};\n" );
}

int test_prng() {

  uint64_t x[10];

  if( prng_footprint()!=(uint64_t)sizeof (prng_t) ) { printf( "FAIL (footprint)\n" ); return 1; }
  if( prng_align    ()!=(uint64_t)alignof(prng_t) ) { printf( "FAIL (align)\n"     ); return 1; }

  prng_t _prng[1];
  prng_t * prng = prng_join( prng_new( _prng, (uint32_t)0, (uint64_t)0 ) );

  if( prng_seq( prng )!=(uint32_t)0 ) { printf( "FAIL (seq)\n" ); return 1; }
  if( prng_idx( prng )!=(uint64_t)0 ) { printf( "FAIL (idx)\n" ); return 1; }

  for( int i=0; i<10; i++ ) x[i] = prng_uint64( prng );
  for( int i=0; i<10; i++ )
    if( x[i]!=x0_0[i] ) {
      printf( "FAIL(0,0)\n" );
      printf_ref( x0_0, (uint32_t)0, (uint64_t)0 );
      printf_ref( x,    (uint32_t)0, (uint64_t)0 );
      return 1;
    }

  if( prng_seq( prng )!=(uint32_t) 0 ) { printf( "FAIL (seq-1)\n" ); return 1; }
  if( prng_idx( prng )!=(uint64_t)20 ) { printf( "FAIL (idx-1)\n" ); return 1; }

  if( prng_seq_set( prng, (uint32_t)1 )!=(uint32_t)0 ) { printf( "FAIL (seq_set)\n" ); return 1; }

  for( int i=0; i<10; i++ ) x[i] = prng_uint64( prng );
  for( int i=0; i<10; i++ )
    if( x[i]!=x1_20[i] ) {
      printf( "FAIL(1,20)\n" );
      printf_ref( x1_20, (uint32_t)1, (uint64_t)20 );
      printf_ref( x,     (uint32_t)1, (uint64_t)20 );
      return 1;
    }

  if( prng_seq( prng )!=(uint32_t) 1 ) { printf( "FAIL (seq-2)\n" ); return 1; }
  if( prng_idx( prng )!=(uint64_t)40 ) { printf( "FAIL (idx-2)\n" ); return 1; }

  if( prng_seq_set( prng, (uint32_t)0 )!=(uint32_t) 1 ) { printf( "FAIL (seq_set-1)\n" ); return 1; }
  if( prng_idx_set( prng, (uint64_t)0 )!=(uint64_t)40 ) { printf( "FAIL (idx_set-1)\n" ); return 1; }

  for( int i=0; i<10; i++ ) x[i] = prng_uint64( prng );
  for( int i=0; i<10; i++ ) if( x[i]!=x0_0[i] ) { printf( "FAIL(0,0)-1\n" ); return 1; }

  uint64_t domain = (uint64_t)0;
  for( int i=0; i<1000; i++ ) {
    uint8_t  u8  = prng_uint8    ( prng );
    uint16_t u16 = prng_uint16   ( prng );
    uint32_t u32 = prng_uint32   ( prng );
    uint64_t u64 = prng_uint64   ( prng );

    int8_t   i8  = prng_int8     ( prng ); if( i8 <(int8_t )0           ) { printf( "FAIL (int8 )\n" ); return 1; }
    int16_t  i16 = prng_int16    ( prng ); if( i16<(int16_t)0           ) { printf( "FAIL (int16)\n" ); return 1; }
    int32_t  i32 = prng_int32    ( prng ); if( i32<(int32_t)0           ) { printf( "FAIL (int32)\n" ); return 1; }
    int64_t  i64 = prng_int64    ( prng ); if( i64<(int64_t)0           ) { printf( "FAIL (int64)\n" ); return 1; }

    float    fc0 = prng_float_c0 ( prng ); if( !( 0.f<=fc0 && fc0< 1.f) ) { printf( "FAIL (float_c0)\n"  ); return 1; }
    float    fc1 = prng_float_c1 ( prng ); if( !( 0.f< fc1 && fc1<=1.f) ) { printf( "FAIL (float_c1)\n"  ); return 1; }
    float    fc  = prng_float_c  ( prng ); if( !( 0.f<=fc  && fc <=1.f) ) { printf( "FAIL (float_c)\n"   ); return 1; }
    float    fnc = prng_float_o  ( prng ); if( !( 0.f< fnc && fnc< 1.f) ) { printf( "FAIL (float_o)\n"   ); return 1; }

    double   dc0 = prng_double_c0( prng ); if( !( 0.f<=dc0 && dc0< 1.f) ) { printf( "FAIL (double_c0)\n" ); return 1; }
    double   dc1 = prng_double_c1( prng ); if( !( 0.f< dc1 && dc1<=1.f) ) { printf( "FAIL (double_c1)\n" ); return 1; }
    double   dc  = prng_double_c ( prng ); if( !( 0.f<=dc  && dc <=1.f) ) { printf( "FAIL (double_c)\n"  ); return 1; }
    double   dnc = prng_double_o ( prng ); if( !( 0.f< dnc && dnc< 1.f) ) { printf( "FAIL (double_o)\n"  ); return 1; }

#   define LO(i,w) ((uint64_t)((uint64_t)(i >> (w-4))==(uint64_t)0x0))
#   define HI(i,w) ((uint64_t)((uint64_t)(i >> (w-4))==(uint64_t)0xf))

    domain |= LO(u8, 8 ) <<  0; domain |= HI(u8, 8 ) <<  4;
    domain |= LO(u16,16) <<  1; domain |= HI(u16,16) <<  5;
    domain |= LO(u32,32) <<  2; domain |= HI(u32,32) <<  6;
    domain |= LO(u64,64) <<  3; domain |= HI(u64,64) <<  7;

    domain |= LO(i8, 7 ) <<  8; domain |= HI(i8, 7 ) << 12;
    domain |= LO(i16,15) <<  9; domain |= HI(i16,15) << 13;
    domain |= LO(i32,31) << 10; domain |= HI(i32,31) << 14;
    domain |= LO(i64,63) << 11; domain |= HI(i64,63) << 15;

#   undef HI
#   undef LO
#   define LO(f) ((uint64_t)(f<0.0625f))
#   define HI(f) ((uint64_t)(f>0.9375f))

    domain |= LO(fc0)    << 16; domain |= HI(fc0)    << 20;
    domain |= LO(fc1)    << 17; domain |= HI(fc1)    << 21;
    domain |= LO(fc )    << 18; domain |= HI(fc )    << 22;
    domain |= LO(fnc)    << 19; domain |= HI(fnc)    << 23;

#   undef HI
#   undef LO
#   define LO(d) ((uint64_t)(d<0.0625))
#   define HI(d) ((uint64_t)(d>0.9375))

    domain |= LO(dc0)    << 24; domain |= HI(dc0)    << 28;
    domain |= LO(dc1)    << 25; domain |= HI(dc1)    << 29;
    domain |= LO(dc )    << 26; domain |= HI(dc )    << 30;
    domain |= LO(dnc)    << 27; domain |= HI(dnc)    << 31;

#   undef HI
#   undef LO
  }
  if( domain!=((UINT64_C(1)<<32)-UINT64_C(1)) ) { printf( "FAIL (domain)\n" ); return 1; }

  prng_delete( prng_leave( prng ) );

# if 0
  long sum_pop  = 0L;
  long sum_pop2 = 0L;
  int  min_pop  = INT_MAX;
  int  max_pop  = INT_MIN;
  for( long i=0; i<(1L<<32); i++ ) {
    uint64_t seq = prng_private_expand( (uint32_t)i );
    if( prng_private_contract( seq )!=(uint32_t)i ) { printf( "FAIL (contract)\n" ); return 1; }
    int pop  = __builtin_popcountl( seq );
    sum_pop  += (long) pop;
    sum_pop2 += (long)(pop*pop);
    min_pop  = pop<min_pop ? pop : min_pop;
    max_pop  = pop>max_pop ? pop : max_pop;
  }
  double avg_pop = ((double)sum_pop ) / ((double)(1L<<32));
  double rms_pop = sqrt( (((double)sum_pop2) / ((double)(1L<<32))) - avg_pop*avg_pop );
  printf( "expand popcount stats: %.3f +/- %.3f [%i,%i]\n", avg_pop, rms_pop, min_pop, max_pop );
# endif

  printf( "pass\n" );
  return 0;
}
