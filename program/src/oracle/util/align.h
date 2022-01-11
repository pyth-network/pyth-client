#ifndef _pyth_oracle_util_align_h_
#define _pyth_oracle_util_align_h_

#include <stdint.h>   /* For uintptr_t */
#include <stdalign.h> /* For alignof */

#ifdef __cplusplus
extern "C" {
#endif

/* align_ispow2 returns non-zero if a is a non-negative integral power
   of 2 and zero otherwise. */

static inline int
align_ispow2( uintptr_t a ) {
  uintptr_t mask = a - (uintptr_t)1;
  return (a>(uintptr_t)0) & !(a & mask);
}

/* align_isaligned returns non-zero if p has byte alignment a.  Assumes
   align_ispow2(a) is true. */

static inline int
align_isaligned( void *    p,
                 uintptr_t a ) {
  uintptr_t mask = a - (uintptr_t)1;
  return !(((uintptr_t)p) & mask);
}

/* align_dn/align_up aligns p to the first byte at or before / after p
   with alignment a.  Assumes align_ispow2(a) is true. */

static inline void *
align_dn( void *    p,
          uintptr_t a ) {
  uintptr_t mask = a - (uintptr_t)1;
  return (void *)(((uintptr_t)p) & (~mask));
}

static inline void *
align_up( void *    p,
          uintptr_t a ) {
  uintptr_t mask = a - (uintptr_t)1;
  return (void *)((((uintptr_t)p) + mask) & (~mask));
}

/* Helper macros for aligning pointers up or down to compatible
   alignments for type T.  FIXME: ADD UNIT TEST COVERAGE */

#define ALIGN_DN( p, T ) ((T *)align_dn( p, (uintptr_t)alignof(T) ))
#define ALIGN_UP( p, T ) ((T *)align_up( p, (uintptr_t)alignof(T) ))

#ifdef __cplusplus
}
#endif

#endif /* _pyth_oracle_util_align_h_ */
