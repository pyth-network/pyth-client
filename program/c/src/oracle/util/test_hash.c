#include <stdio.h>
#include "util.h"

uint32_t ref32[10] = {
  UINT32_C( 0x00000000 ),
  UINT32_C( 0x514e28b7 ),
  UINT32_C( 0x30f4c306 ),
  UINT32_C( 0x85f0b427 ),
  UINT32_C( 0x249cb285 ),
  UINT32_C( 0xcc0d53cd ),
  UINT32_C( 0x5ceb4d08 ),
  UINT32_C( 0x18c9aec4 ),
  UINT32_C( 0x4939650b ),
  UINT32_C( 0xc27c2913 )
};


uint64_t ref64[10] = {
  UINT64_C( 0x0000000000000000 ),
  UINT64_C( 0xb456bcfc34c2cb2c ),
  UINT64_C( 0x3abf2a20650683e7 ),
  UINT64_C( 0x0b5181c509f8d8ce ),
  UINT64_C( 0x47900468a8f01875 ),
  UINT64_C( 0xd66ad737d54c5575 ),
  UINT64_C( 0xe8b4b3b1c77c4573 ),
  UINT64_C( 0x740729cbe468d1dd ),
  UINT64_C( 0x46abcca593a3c687 ),
  UINT64_C( 0x91209a1ff7f4f1d5 )
};

int
main( int     argc,
      char ** argv ) {
  (void)argc; (void)argv;

  for( int i=0; i<10; i++ ) {
    uint32_t x = (uint32_t)i;
    uint32_t y = hash_uint32( x );
    uint32_t z = hash_inverse_uint32( y );
    if( y!=ref32[i] ) { printf( "ref32: FAIL\n" ); return 1; }
    if( x!=z        ) { printf( "inv32: FAIL\n" ); return 1; }
    if( hash_uint32( hash_inverse_uint32( x ) )!=x ) { printf( "INV32: FAIL\n" ); return 1; }
  }

  for( int i=0; i<10; i++ ) {
    uint64_t x = (uint64_t)i;
    uint64_t y = hash_uint64( x );
    uint64_t z = hash_inverse_uint64( y );
    if( y!=ref64[i] ) { printf( "ref64: FAIL\n" ); return 1; }
    if( x!=z        ) { printf( "inv64: FAIL\n" ); return 1; }
    if( hash_uint64( hash_inverse_uint64( x ) )!=x ) { printf( "INV64: FAIL\n" ); return 1; }
  }

  /* FIXME: MEASURE AVALANCHE PROPERTIES? */

  printf( "pass\n" );
  return 0;
}

