/// The goal of this file is to provide the upd_aggregate function to local rust tests

/// We need to allocate some heap space for upd_aggregate
/// When compiling for the solana runtime, the heap space is preallocated and PC_HEAP_START is provided by <solana.h>
char heap_start[8192];
#define PC_HEAP_START (heap_start)
#define static_assert _Static_assert

#include "../upd_aggregate.h"
#include "features.h"

#ifdef PC_PYTHNET
extern bool c_upd_aggregate_pythnet( pc_price_t *ptr, uint64_t slot, int64_t timestamp ){
#else
extern bool c_upd_aggregate_solana( pc_price_t *ptr, uint64_t slot, int64_t timestamp ){
#endif
  return upd_aggregate(ptr, slot, timestamp );
}

extern void c_upd_twap( pc_price_t *ptr, int64_t nslots ){
  upd_twap(ptr, nslots);
}
