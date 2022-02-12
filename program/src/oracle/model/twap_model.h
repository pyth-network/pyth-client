#ifndef _pyth_oracle_model_twap_model_h_
#define _pyth_oracle_model_twap_model_h_

#include "../util/compat_stdint.h"
#include <stdalign.h>

/* twap_model_t should be treated as an opaque handle of a twap_model.
   (It technically isn't here to facilitate inlining of operations.) */

struct twap_model {
  int8_t   valid;           /* If 0, all values below should be ignored */
  uint64_t ts;              /* Block sequence number corresponding to the below values */
  uint64_t twap;            /* round( twap_N/D ) */
  uint64_t twac;            /* round( twac_N/D ) */
  uint64_t twap_Nh,twap_Nl; /* twap_N is the weighted sum of recent aggregrated price observations */
  uint64_t twac_Nh,twac_Nl; /* twac_N is the weighted sum of recent aggregrated confidence observations */
  uint64_t D;               /* D      is the weighted number of recent observations */
};

typedef struct twap_model twap_model_t;

#ifdef __cplusplus
extern "C" {
#endif

/* twap_model_footprint and twap_model_align give the needed footprint
   and alignment for a memory region to hold a twap_model's state.
   malloc / alloca / declaration friendly (e.g. a memory region declared
   as "twap_model_t _tm[1];", or created by
   "malloc(sizeof(twap_model_t))" or created by
   "alloca(sizeof(twap_model_t))" will all automatically have the needed
   footprint and alignment).

   twap_model_new takes ownership of the memory region pointed to by
   shmem (which is assumed to be non-NULL with the appropriate footprint
   and alignment) and formats it as a twap_model.  Returns mem (which
   will be formatted for use).  The caller will not be joined to the
   region on return.

   twap_model_join joins the caller to a memory region holding the state
   of a twap_model.  shtm points to a memory region in the local address
   space that holds the twap_model.  Returns an opaque handle of the
   local join in the local address space to twap_model (which might not
   be the same thing as shtm ... the separation of new and join is to
   facilitate interprocess shared memory usage patterns while supporting
   transparent upgrades users of this to more elaborate models where
   address translations under the hood may not be trivial).

   twap_model_leave leaves the current twap_model join.  Returns a
   pointer in the local address space to the memory region holding the
   state of the twap_model.  The twap_model join should not be used
   afterward.

   twap_model_delete unformats the memory region currently used to hold
   the state of a _twap_model and returns ownership of the underlying
   memory region to the caller.  There should be no joins in the system
   on the twap_model.  Returns a pointer to the underlying memory
   region. */

static inline uint64_t twap_model_footprint( void ) { return (uint64_t) sizeof(twap_model_t); }
static inline uint64_t twap_model_align    ( void ) { return (uint64_t)alignof(twap_model_t); }

static inline void *
twap_model_new( void * shmem ) {
  twap_model_t * tm = (twap_model_t *)shmem;
  tm->valid   = INT8_C(0);
  tm->ts      = UINT64_C(0);
  tm->twap    = UINT64_C(0);
  tm->twac    = UINT64_C(0);
  tm->twap_Nh = UINT64_C(0); tm->twap_Nl = UINT64_C(0);
  tm->twac_Nh = UINT64_C(0); tm->twac_Nl = UINT64_C(0);
  tm->D       = UINT64_C(0);
  return tm;
}

static inline twap_model_t * twap_model_join  ( void *         shtm ) { return (twap_model_t *)shtm; }
static inline void *         twap_model_leave ( twap_model_t * tm   ) { return tm; }
static inline void *         twap_model_delete( void *         shtm ) { return shtm; }

/* Accessors to the current state of a twap model. */

static inline int      twap_model_valid( twap_model_t const * tm ) { return !!tm->valid; }
static inline uint64_t twap_model_ts   ( twap_model_t const * tm ) { return tm->ts;      }
static inline uint64_t twap_model_twap ( twap_model_t const * tm ) { return tm->twap;    }
static inline uint64_t twap_model_twac ( twap_model_t const * tm ) { return tm->twac;    }

/* Update the state of the twap_model to incorporate observation
   at block chain sequence number now the aggregrate price / confidence
   pair (agg_price,agg_conf).  Returns tm.  */

twap_model_t *
twap_model_update( twap_model_t * tm,
                   uint64_t       now,
                   uint64_t       agg_price,
                   uint64_t       agg_conf );

#ifdef __cplusplus
}
#endif

#endif /* _pyth_oracle_model_twap_model_h_ */

