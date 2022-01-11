#include <stdio.h>
#include <stdlib.h>
#include <bbattery.h> /* Assumes TestU01 install include directory is in the include search path */
#include "util.h"

static double
test_GetU01( void * param,
             void * state ) {
  (void)param;
  return 2.3283064365386962890625e-10 /* 2^-32, exact */ * (double)prng_uint32( (prng_t *)state );
}

static unsigned long
test_GetBits( void * param,
              void * state ) {
  (void)param;
  return (unsigned long)prng_uint32( (prng_t *)state );
}

static void
test_Write( void * state ) {
  prng_t * prng = (prng_t *)state;
  printf( "prng(0x%08lxU,0x%016lxUL)\n", (unsigned long)prng_seq( prng ), (unsigned long)prng_idx( prng ) );
}

static void
usage( char const * cmd ) {
  fprintf( stderr,
           "Usage: %s [bat] [seq] [idx]\n"
           "\tbat 0 - FIPS-140.2 (fast)\n"
           "\tbat 1 - Pseudo-DIEHARD (fast)\n"
           "\tbat 2 - TestU01 SmallCrush (fast)\n"
           "\tbat 3 - TestU01 Crush (~20 minutes)\n"
           "\tbat 4 - TestU01 BigCrush (several hours)\n"
           "Note that when running a test in a battery, the probability of it failing even\n"
           "though the generator is fine is roughly 0.2%%.  Thus, when repeatedly running\n"
           "batteries that themselves contain a large number of tests, some test failures\n"
           "are expected.  So long as the test failures are are sporadic, don't occur in the\n"
           "same place when running multiple times with random seq and/or idx, and don't\n"
           "have p-values improbably close to 0 (p <<< 1/overall_num_tests_run) or 1\n"
           "(1-p <<< 1/overall_num_tests_run), it is expected and normal\n",
           cmd );
}

int
main( int     argc,
      char ** argv ) {

  /* Get command line arguments */

  if( argc!=4 ) { usage( argv[0] ); return 1; }
  int      bat = atoi( argv[1] );
  uint32_t seq = (uint32_t)strtoul( argv[2], NULL, 0 );
  uint64_t idx = (uint64_t)strtoul( argv[3], NULL, 0 );

  /* Create the test generator */

  prng_t _prng[1];
  prng_t * prng = prng_join( prng_new( _prng, seq, idx ) );

  /* Run the requested test battery */

  char name[128];
  sprintf( name, "prng(0x%08lxU,0x%016lxUL)", (unsigned long)prng_seq( prng ), (unsigned long)prng_idx( prng ) );

  unif01_Gen gen[1];
  gen->state   = prng;
  gen->param   = NULL;
  gen->name    = name;
  gen->GetU01  = test_GetU01;
  gen->GetBits = test_GetBits;
  gen->Write   = test_Write;

  switch( bat ) {
  case 0: bbattery_FIPS_140_2( gen );    break;
  case 1: bbattery_pseudoDIEHARD( gen ); break;
  case 2: bbattery_SmallCrush( gen );    break;
  case 3: bbattery_Crush( gen );         break;
  case 4: bbattery_BigCrush( gen );      break;
//case 5: bbattery_Rabbit( gen );        break; /* FIXME: NB */
//case 6: bbattery_Alphabit( gen );      break; /* FIXME: NB/R/S */
//case 7: bbattery_BlockAlphabit( gen ); break; /* FIXME: NB/R/S */
  default: usage( argv[0] ); return 1;
  }

  /* Destroy the test generator */

  prng_delete( prng_leave( prng ) );

  return 0;
}

