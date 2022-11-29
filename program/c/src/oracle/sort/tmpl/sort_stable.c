/* Usage:

     #define SORT_NAME  mysort
     #define SORT_KEY_T mykey_t
     #include "sort_stable_impl.c"

   This will instantiate the following APIs:

      // Returns non-zero if n is a supported sort size and zero if not.
      // Unsupported values are negative n (only applicable for signed
      // indexing types) or unreasonably large n (such that the scratch
      // space requirement would be bigger than UINT64_MAX).

      static inline int
      mysort_stable_cnt_valid( uint64_t cnt );

      // Return the footprint required for a scratch space of any
      // alignment sufficient for sorting up to n items inclusive.
      // Returns 0 if cnt is not valid or no scratch is needed.

      static inline uint64_t
      mysort_stable_scratch_footprint( uint64_t cnt );

      // Sort elements of keys into an ascending order.  Algorithm has a
      // best case of ~0.5 cnt lg cnt and an average and worst case of
      // cnt lg cnt such that it is moderately resistant to timing and
      // computational DOS attacks.  Further, the sort is stable.  The
      // values in scratch are irrelevant on input.  Returns where the
      // sorted data ended up (either key or ALIGN_UP(scratch,mykey_t)).
      // That is, if this returns key, the values in key are the stably
      // sorted data and scratch was clobbered.  Otherwise, the values
      // at ALIGN_UP(scratch,mykey_t) are the stably sorted data and key
      // was clobbered.  Users wanting the data in a particular location
      // can copy as necessary (allowing this flexibility minimizes the
      // amount of copying needed to do the sorting).  E.g.:
      //
      //   mykey_t * tmp = mysort_stable( key, cnt, scratch );
      //   if( tmp!=key ) memcpy( key, tmp, cnt*sizeof(mykey_t) );
      //
      // scratch points to a scratch memory region of any alignment with
      // room for mysort_stable_scratch_footprint( cnt ) bytes.  (Any
      // normally declared / normally allocated region with mykey_t
      // compatible alignment and space for cnt mykey_t's will work
      // too.)
      //
      // FIXME: CONSIDER RETURNING NULL IF BAD INPUT ARGS

      static mykey_t *
      mysort_stable( mykey_t * key,       // Indexed [0,n)
                     uint64_t  cnt,       // Assumes mysort_stable_cnt_valid( cnt ) is true
                     void *    scratch ); // Pointer to suitable scratch region

   This can be included multiple types with different names / parameters
   to define many family of sorts that might be useful for a compilation
   unit.

   Other defines exist to change the sort criteria / direction, linkage
   and so forth.  See below for details. */

#include "../../util/compat_stdint.h" /* For uint64_t */
#include "../../util/align.h"         /* For ALIGN_UP */

#ifndef SORT_NAME
#error "Define SORT_NAME"
#endif

#ifndef SORT_KEY_T
#error "Define SORT_KEY_T; nominally a POD (plain-old-data) type"
#endif

/* Define SORT_IDX_T to specify the data type used to index key arrays.
   Default is uint64_t. */

#ifndef SORT_IDX_T
#define SORT_IDX_T uint64_t
#endif

/* Define SORT_BEFORE to specify how sorted keys should be ordered.
   Default is ascending as defined by the "<" operator for the type.
   SORT_BEFORE(u,v) should be non-zero if key u should go strictly
   before key v and zero otherwise. */

#ifndef SORT_BEFORE
#define SORT_BEFORE(u,v) ((u)<(v))
#endif

/* Define SORT_STATIC to specify the type of linkage the non-inlined
   APIs should have (e.g. if defined to nothing, these will have
   external linkage).  Default is static linkage. */

#ifndef SORT_STATIC
#define SORT_STATIC static
#endif

/* Define SORT_STATIC_INLINE to specify the type of linkage inlined
   APIs should have (e.g. if defined to nothing, these will have
   non-inlined external linkage).  Default is static inline linkage. */

#ifndef SORT_STATIC_INLINE
#define SORT_STATIC_INLINE static inline
#endif

/* Some macro preprocessor helpers */

#define SORT_C3(a,b,c)a##b##c
#define SORT_XC3(a,b,c)SORT_C3(a,b,c)
#define SORT_IMPL(impl)SORT_XC3(SORT_NAME,_,impl)

SORT_STATIC_INLINE int
SORT_IMPL(stable_cnt_valid)( SORT_IDX_T cnt ) {
  /* Written this way for complier warning free signed SORT_IDX_T and/or
     byte size SORT_KEY_T support (e.g. compiler often will warn to the
     effect "n>=0 always true" if idx is an unsigned type or
     "n<=UINT64_MAX always true" if key is a byte type). */
  static uint64_t const max = ((UINT64_MAX - (uint64_t)alignof(SORT_KEY_T) + (uint64_t)1) / (uint64_t)sizeof(SORT_KEY_T));
  return !cnt || (((SORT_IDX_T)0)<cnt && ((uint64_t)cnt)<max) || ((uint64_t)cnt)==max;
}

SORT_STATIC_INLINE uint64_t
SORT_IMPL(stable_scratch_footprint)( SORT_IDX_T cnt ) {
  if( !SORT_IMPL(stable_cnt_valid)( cnt ) ) return (uint64_t)0;
  /* Guaranteed not to overflow given a valid cnt */
  return ((uint64_t)sizeof (SORT_KEY_T))*(uint64_t)cnt /* Space for the n SORT_KEY_T's */
       + ((uint64_t)alignof(SORT_KEY_T))-(uint64_t)1;  /* Worst case alignment padding */
}

SORT_STATIC SORT_KEY_T *
SORT_IMPL(stable_node)( SORT_KEY_T * x,
                        SORT_IDX_T   n,
                        SORT_KEY_T * t ) {

  /* Optimized handling of base cases */

# include "sort_stable_base.c"

  /* Note that n is at least 2 at this point */
  /* Break input into approximately equal halves and sort them */

  SORT_KEY_T * xl = x;
  SORT_KEY_T * tl = t;
  SORT_IDX_T   nl = n >> 1;
  SORT_KEY_T * yl = SORT_IMPL(stable_node)( xl,nl, tl );

  SORT_KEY_T * xr = x + nl;
  SORT_KEY_T * tr = t + nl;
  SORT_IDX_T   nr = n - nl;
  SORT_KEY_T * yr = SORT_IMPL(stable_node)( xr,nr, tr );

  /* If left subsort result ended up in orig array, merge into temp
     array.  Otherwise, merge into orig array. */

  if( yl==xl ) x = t;

  /* At this point, note that yl does not overlap with the location for
     merge output at this point.  yr might overlap (with the right half)
     with the location for merge output but this will still work in that
     case. */

  SORT_IDX_T i = (SORT_IDX_T)0;
  SORT_IDX_T j = (SORT_IDX_T)0;
  SORT_IDX_T k = (SORT_IDX_T)0;

  /* Note that nl and nr are both at least one at this point so at least
     one iteration of the loop body is necessary. */

  for(;;) { /* Minimal C language operations */
    if( SORT_BEFORE( yr[k], yl[j] ) ) {
      x[i++] = yr[k++];
      if( k>=nr ) { /* append left  stragglers (at least one) */ do x[i++] = yl[j++]; while( j<nl ); break; }
    } else {
      x[i++] = yl[j++];
      if( j>=nl ) { /* append right stragglers (at least one) */ do x[i++] = yr[k++]; while( k<nr ); break; }
    }
  }

  return x;
}

SORT_STATIC_INLINE SORT_KEY_T *
SORT_IMPL(stable)( SORT_KEY_T * key,
                   SORT_IDX_T   cnt,        /* Assumed valid cnt */
                   void       * scratch ) {
  return SORT_IMPL(stable_node)( key, cnt, ALIGN_UP( scratch, SORT_KEY_T ) );
}

#undef SORT_IMPL
#undef SORT_XC3
#undef SORT_C3

#undef SORT_STATIC_INLINE
#undef SORT_STATIC
#undef SORT_BEFORE
#undef SORT_IDX_T
#undef SORT_KEY_T
#undef SORT_NAME
