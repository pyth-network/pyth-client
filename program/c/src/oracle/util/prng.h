#ifndef _pyth_oracle_util_prng_h_
#define _pyth_oracle_util_prng_h_

/* Simple fast high quality non-cryptographic pseudo random number
   generator.  Supports parallel generation, interprocess shared memory
   usage, checkpointing, random access, reversible, atomic, etc.  Passes
   extremely strict tests of randomness.

   Assumes hash.h provides a high quality 64<>64-bit integer hash
   functions (i.e. full avalanche) with the property hash_uint64(0)==0
   and hash_uint64(i) for i in [0,2^64) yields a permutation of [0,2^64)
   and also a reasonable efficient inverse. */

#include <stdalign.h>
#include "hash.h" /* includes stdint.h */

/* prng_t should be treated as an opaque handle of a pseudo random
   number generator.  (It technically isn't here to faciliate inlining
   of prng operations.) */

struct prng {
  uint64_t seq;
  uint64_t idx;
};

typedef struct prng prng_t;

#ifdef __cplusplus
extern "C" {
#endif

/* Private: randomly expands an arbitrary 32-bit value into a unique
   64-bit non-sparse value such that the original 32-bit value can be
   recovered and that 0 expands to something non-zero (the non-sparse
   expansion helps reduce correlations between different sequences, the
   zero to non-zero expansion means the common initialization of seq=0,
   idx=0 doesn't yield 0 for the first random value as would happen for
   hash functions that have the property hash_uint64(0)==0, the XOR
   64-bit const here is zero in the lower 32-bits and an arbitrary
   non-zero in the upper 32-bits).  For the current hash_uint64
   implementation and XOR 64-bit constant, the pop count of the expanded
   seq is ~32.000 +/- ~4.000 and in [8,56] for all possible values of
   seq (i.e. the expanded seq popcount is well approximated as a normal
   with mean 32 and rms 4 and the extremes are in line with the expected
   extremes for 2^32 samples). */

static inline uint64_t
prng_private_expand( uint32_t seq ) {
  return hash_uint64( UINT64_C( 0x900df00d00000000 ) ^ (uint64_t)seq );
}

/* Private: extract the original 32-bit seq from its expanded value */

static inline uint32_t
prng_private_contract( uint64_t seq ) {
  return (uint32_t)hash_inverse_uint64( seq );
}

/* prng_footprint and prng_align give the needed footprint and alignment
   for a memory region to hold a prng's state.  malloc / alloca /
   declaration friendly (e.g. a memory region declared as "prng_t
   _prng[1];", or created by "malloc(sizeof(prng_t))" or created by
   "alloca(sizeof(prng_t))" will all automatically have the needed
   footprint and alignment).

   prng_new takes ownership of the memory region pointed to by mem
   (which is assumed to be non-NULL with the appropriate footprint and
   alingment) and formats it as a prng.  The pseudo random number
   generator stream will initialized to use sequence random sequence
   "seq" and will start at index "idx".  Returns mem (which will be
   formatted for use).  The caller will not be joined to the region on
   return.

   prng_join joins the caller to a memory region holding the state of a
   prng.  _prng points to a memory region in the local address space
   that holds prng.  Returns an opaque handle of the local join in the
   local address space to the prng (which might not be the same thing as
   _prng ... the separation of new and join is to facilitate
   interprocess shared memory usage patterns while supporting
   transparent upgrades users of this to more elaborate PRNG algorithms
   where address translations under the hood may not be trivial).

   prng_leave leaves the current prng join.  Returns a pointer in the
   local address space to the memory region holding the state of the
   prng.  The prng join should not be used afterward.

   prng_delete unformats the memory region currently used to hold the
   state of a _prng and returns ownership of the underlying memory
   region to the caller.  There should be no joins in the system on the
   prng.  Returns a pointer to the underlying memory region.

   FIXME: CONSIDER FLATTENING ALIGN? */

static inline uint64_t prng_footprint( void ) { return (uint64_t)sizeof ( prng_t ); }
static inline uint64_t prng_align    ( void ) { return (uint64_t)alignof( prng_t ); }

static inline void *
prng_new( void *   mem,
          uint32_t seq,
          uint64_t idx ) {
  prng_t * prng = (prng_t *)mem;
  prng->seq = prng_private_expand( seq );
  prng->idx = idx;
  return (void *)prng;
}

static inline prng_t * prng_join  ( void   * _prng ) { return (prng_t *)_prng; }
static inline void   * prng_leave ( prng_t *  prng ) { return (void   *) prng; }
static inline void *   prng_delete( void   * _prng ) { return (void   *)_prng; }

/* prng_seq returns the sequence used by the prng.  prng_idx returns the
   next index that will be consumed by the prng. */

static inline uint32_t prng_seq( prng_t * prng ) { return prng_private_contract( prng->seq ); }
static inline uint64_t prng_idx( prng_t * prng ) { return prng->idx;                          }

/* prng_seq_set sets the sequence to be used by the prng and returns the
   replaced value.  prng_idx_set sets the next index that will be
   consumed by the prng and returns the replaced value. */

static inline uint32_t
prng_seq_set( prng_t * prng,
              uint32_t seq ) {
  uint32_t old = prng_seq( prng );
  prng->seq = prng_private_expand( seq );
  return old;
}

static inline uint64_t
prng_idx_set( prng_t * prng,
              uint64_t idx ) {
  uint64_t old = prng_idx( prng );
  prng->idx = idx;
  return old;
}

/* prng_uintN returns the next integer in the PRNG sequence in [0,2^N)
   with uniform probability with a period of 2^64 (prng_uint64 has a
   period of 2^63 as consumes two prng indices).  Passes various strict
   PRNG tests (e.g. diehard, dieharder, testu01, etc).  Assumes prng is
   a current join.  prng_intN is the same as prng_uintN but returns a
   value in [0,2^(N-1)).  (A signed generator that can assume all
   possible values of of a signed int uniform IID can be obtained by
   casting the output of the unsigned generator of the same, assuming
   typical twos complement arithmetic platform.)

   The theory for this that hash_uint64(i) for i in [0,2^64) specifies a
   random looking permutation of the integers in [0,2^64).  Returning
   the low order bits of this random permutation then yields a high
   quality non-cryptographic random number stream automatically as it:

   - Has a long period (2^64).  This is implied by the permutation
     property.

   - Has the expected random properties (as theoretically best possible
     for a finite period generator) of a true uniform IID bit source.
     For example, the probability of next random number is uniform and
     independent of previous N random numbers for N<<2^64).  This is
     also implied by the full avalanche and permutation property.

   - Is "unpredictable".  That is, the internal state of the generator
     is difficult to recover from its outputs, e.g. a return from
     prng_uint32 could be have been generated from 2^32 internal states
     (if the sequence is known), up to 2^32 sequences (if the state is
     known) and up to 2^64 (state,seq) pairs neither is known (the state
     / sequence is potentially recoverable given a long enough stream of
     values though).  This is implied by the truncation of hash values.

   To turn this into parameterizable family of generators, note that
   hash_uint64( perm_j( i ) ) where j is some parameterized family of
   random permutations is still a permutation and would have all the
   above properties for free so long as no perm_j is similar to the
   inverse of the hash permutation.  Practically, xoring i by a
   non-sparse 64-bit number will ultra cheaply generate a wide family of
   "good enough" permutations to do a parameterized shuffling of the
   original hash_uint64 permutation, creating a large number of parallel
   sequences.  Since users are empirically notoriously bad at seeding
   though, we only let the user specify a 32-bit sequence id and then
   generate a unique non-sparse 64-bit random looking number from it. */

static inline uint8_t  prng_uint8 ( prng_t * prng ) { return (uint8_t )hash_uint64( prng->seq ^ (prng->idx++) ); }
static inline uint16_t prng_uint16( prng_t * prng ) { return (uint16_t)hash_uint64( prng->seq ^ (prng->idx++) ); }
static inline uint32_t prng_uint32( prng_t * prng ) { return (uint32_t)hash_uint64( prng->seq ^ (prng->idx++) ); }

static inline uint64_t
prng_uint64( prng_t * prng ) {
  uint64_t hi = (uint64_t)prng_uint32( prng );
  return (hi<<32) | (uint64_t)prng_uint32( prng );
}

static inline int8_t  prng_int8 ( prng_t * prng ) { return (int8_t )( prng_uint8 ( prng ) >> 1 ); }
static inline int16_t prng_int16( prng_t * prng ) { return (int16_t)( prng_uint16( prng ) >> 1 ); }
static inline int32_t prng_int32( prng_t * prng ) { return (int32_t)( prng_uint32( prng ) >> 1 ); }
static inline int64_t prng_int64( prng_t * prng ) { return (int64_t)( prng_uint64( prng ) >> 1 ); }

/* These quickly and robustly convert uniform random integers into
   uniform random floating point with appropriate rounding.  Intervals
   are:

     *_o  -> (0,1)
     *_c0 -> [0,1)
     *_c1 -> (0,1]
     *_c  -> [0,1]

   To provide more specifics, let the real numbers from [0,1) be
   partitioned into N uniform disjoint intervals such that interval i
   goes from [i/N,(i+1)/N) where i is in [0,N).  For single (double)
   precision, "float" ("double"), the largest N for which the range of
   each interval is _exactly_ representable is N = 2^24 (2^53).

   Given then a uniform IID uint32_t random input, the
   prng_uint32_to_float_c0 converter output is as though an continuous
   IID uniform random in [0,1) was generated and then rounded down to
   the start of the containing interval (2^24 intervals).  As such, this
   generator can never return exactly 1 but it can exactly return +0.
   Since floats have higher resolution near 0 than 1, this will not
   return all float possible representations in [0,1) but it can return
   all possible float representations in [1/2,1).  In particular, this
   will never return a denorm or -0.

   Similarly for prng_uint32_to_float_c1 converter rounds up to the end
   of the containing interval / start of the next interval (2^24
   intervals).  As such, this converter can never return exactly +0 but
   it can exactly return 1.  It will not return all possible float
   representations in (0,1] but it can return all possible float
   representations in [1/2,1].  This will never return a denorm or -0.

   The prng_uint32_to_float_c converter rounds toward nearest even
   toward the start containing interval or start of the next interval
   (2^24 intervals).  As such, this can return both exactly +0 and
   exactly 1 (and the probability of returning exactly +0 == probability
   of exactly 1 == (1/2) probability all other possible return values).
   It will not return all possible float representations in [0,1] but it
   can return all float possible representations in [1/2,1].  This will
   never return a denorm or -0.

   The prng_uint32_to_float_o converter rounds toward the middle of
   containing internal (2^23 intervals ... note that then in a sense
   this converter is 1-bit less accurate than the above).  As such, this
   can neither return +0 nor 1 and will not return all possible float
   representations in (0,1).  This will never return a denorm or -0.

   Similarly for the double variants (*_{c0,c1,c} uses 2^53 intervals
   and o uses 2^52 intervals).

   Note that it is possible to make converters that will handle exactly
   rounding toward all possible floating point representations in [0,1]
   but this are significantly more expensive.

   Assumes IEEE-754 style float and doubles.

   FIXME: ADD UNIT TEST COVERAGE */

static inline float
prng_uint32_to_float_c0( uint32_t u ) {
  return (1.f/(float )(INT32_C(1)<<24))*(float)(int32_t)( u>>(32-24)                 );
}

static inline float
prng_uint32_to_float_c1( uint32_t u ) {
  return (1.f/(float )(INT32_C(1)<<24))*(float)(int32_t)((u>>(32-24))+   UINT32_C(1) );
}

static inline float
prng_uint32_to_float_c( uint32_t u ) {
  return (1.f/(float )(INT32_C(1)<<24))*(float)(int32_t)((u>>(32-24))+(u&UINT32_C(1)));
}

static inline float
prng_uint32_to_float_o( uint32_t u ) {
  return (1.f/(float )(INT32_C(1)<<24))*(float)(int32_t)((u>>(32-24))|   UINT32_C(1) );
}

static inline double
prng_uint64_to_double_c0( uint64_t u ) {
  return (1. /(double)(INT64_C(1)<<53))*(double)(int64_t)( u>>(64-53)                 );
}

static inline double
prng_uint64_to_double_c1( uint64_t u ) {
  return (1. /(double)(INT64_C(1)<<53))*(double)(int64_t)((u>>(64-53))+   UINT64_C(1) );
}

static inline double
prng_uint64_to_double_c( uint64_t u ) {
  return (1. /(double)(INT64_C(1)<<53))*(double)(int64_t)((u>>(64-53))+(u&UINT64_C(1)));
}

static inline double
prng_uint64_to_double_o( uint64_t u ) {
  return (1. /(double)(INT64_C(1)<<53))*(double)(int64_t)((u>>(64-53))|   UINT64_C(1) );
}

static inline float  prng_float_c0 ( prng_t * prng ) { return prng_uint32_to_float_c0 ( prng_uint32( prng ) ); }
static inline float  prng_float_c1 ( prng_t * prng ) { return prng_uint32_to_float_c1 ( prng_uint32( prng ) ); }
static inline float  prng_float_c  ( prng_t * prng ) { return prng_uint32_to_float_c  ( prng_uint32( prng ) ); }
static inline float  prng_float_o  ( prng_t * prng ) { return prng_uint32_to_float_o  ( prng_uint32( prng ) ); }

static inline double prng_double_c0( prng_t * prng ) { return prng_uint64_to_double_c0( prng_uint64( prng ) ); }
static inline double prng_double_c1( prng_t * prng ) { return prng_uint64_to_double_c1( prng_uint64( prng ) ); }
static inline double prng_double_c ( prng_t * prng ) { return prng_uint64_to_double_c ( prng_uint64( prng ) ); }
static inline double prng_double_o ( prng_t * prng ) { return prng_uint64_to_double_o ( prng_uint64( prng ) ); }

/* prng_int32_roll uses the given prng to roll an n-sided die where n
   is the number of sides (assumed to be positive).  That is returns
   uniform IID rand in [0,n) even if n is not an exact power of two.

   Rejection method based.  Specifically, the number of prng slots
   consumed is typically 1 but theoretically might be higher
   occassionally (64-bit wide types consume prng slots twice as fast).

   Deterministic prng slot consumption possible with a slightly more
   approximate implementation (bound the number of iterations such that
   this always consumes a fixed number of slot and accept the
   practically infinitesimal bias when n is not a power of 2).

   FIXME: ADD UNIT TEST COVERAGE. */

static inline uint32_t
prng_private_roll32( prng_t * prng,
                     uint32_t n ) {
  uint32_t r = (-n) % n; /* Compute 2^32 mod n = (2^32 - n) mod n = (-n) mod n, compile time for compile time n */
  uint32_t u; do u = prng_uint32( prng ); while( u<r ); /* Rejection unlikely (highly unlikely for n<<<2^32) */
  /* At this point, u is uniform in [r,2^32) which has an integer
     multiple of n elements (thus u % n is in [0,n) uniform) */
  return u % n;
}

static inline uint64_t
prng_private_roll64( prng_t * prng,
                     uint64_t n ) {
  uint64_t r = (-n) % n; /* Compute 2^64 mod n = (2^64 - n) mod n = (-n) mod n, compile time for compile time n */
  uint64_t u; do u = prng_uint64( prng ); while( u<r ); /* Rejection unlikely (highly unlikely for n<<<2^64) */
  /* At this point, u is uniform in [r,2^64) which has an integer
     multiple of n elements (thus u % n is in [0,n) uniform) */
  return u % n;
}

static inline uint8_t  prng_uint8_roll ( prng_t * prng, uint8_t  n ) { return (uint8_t )prng_private_roll32( prng, (uint32_t)n ); }
static inline uint16_t prng_uint16_roll( prng_t * prng, uint16_t n ) { return (uint16_t)prng_private_roll32( prng, (uint32_t)n ); }
static inline uint32_t prng_uint32_roll( prng_t * prng, uint32_t n ) { return (uint32_t)prng_private_roll32( prng, (uint32_t)n ); }
static inline uint64_t prng_uint64_roll( prng_t * prng, uint64_t n ) { return (uint64_t)prng_private_roll64( prng, (uint64_t)n ); }

static inline int8_t   prng_int8_roll  ( prng_t * prng, int8_t   n ) { return (int8_t  )prng_private_roll32( prng, (uint32_t)n ); }
static inline int16_t  prng_int16_roll ( prng_t * prng, int16_t  n ) { return (int16_t )prng_private_roll32( prng, (uint32_t)n ); }
static inline int32_t  prng_int32_roll ( prng_t * prng, int32_t  n ) { return (int32_t )prng_private_roll32( prng, (uint32_t)n ); }
static inline int64_t  prng_int64_roll ( prng_t * prng, int64_t  n ) { return (int64_t )prng_private_roll64( prng, (uint64_t)n ); }

/* FIXME: COIN TOSSING / BERNOULLI TRIALS */

/* FIXME: ADD EXPONENTIAL AND NORMAL GENERATORS.  EXPONENTIAL IS EASY
   (E.G. -LOGF( PRNG_FLOAT_C1( PRNG ) )) BUT THIS REQUIRES AVAILABILITY
   OF MATH.H (MAYBE DO OUT OF LINE).  NORMAL IS BEST DONE VIA ZIGGURAT
   STYLE REJECTION, WHICH IS BEST DONE OUT OF LINE.  EXP CAN ALSO BE
   DONE WITH ZIGGURAT STYLE REJECTION IF MATH.H FUNCTIONS NOT AVAILABLE
   (AND THEN ALSO BEST DOWN OUT OF LINE). */

/* FIXME: IMPORT ATOMIC VARIANTS FOR REENTRANT USAGE (E.G. ATOMIC_XCHG
   FOR SET, ATOMIC_INC OF INDEX FOR THE RETURN TYPES, CAS STATE UPDATES,
   ETC) */

#ifdef __cplusplus
}
#endif

#endif /* _pyth_oracle_util_prng_h_ */
