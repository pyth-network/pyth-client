/*
 * BPF upd_aggregate binding
 */
#include <solana_sdk.h>
#include "oracle.h"
#include "upd_aggregate.h"

extern bool c_upd_aggregate( pc_price_t *ptr, uint64_t slot, int64_t timestamp ){
  return upd_aggregate(ptr, slot, timestamp );
}

extern void c_upd_twap( pc_price_t *ptr, int64_t nslots ){
  upd_twap(ptr, nslots);
}
