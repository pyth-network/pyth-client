/*
 * BPF upd_aggregate binding
 */
#include <solana_sdk.h>
#include "oracle.h"
#include "upd_aggregate.h"
#include "features.h"

// Dynamically deciding the name prevents linking the wrong C binary flavor
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
