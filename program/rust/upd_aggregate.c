char heap_start[8192];
#define PC_HEAP_START (heap_start)
#define static_assert _Static_assert

typedef signed char int8_t;
typedef unsigned char uint8_t;
typedef signed short int16_t;
typedef unsigned short uint16_t;
typedef signed int int32_t;
typedef unsigned int uint32_t;
typedef signed long int int64_t;
typedef unsigned long int uint64_t;

#include <stdlib.h>
#include "../c/src/oracle/upd_aggregate.h"

extern bool c_upd_aggregate( pc_price_t *ptr, uint64_t slot, int64_t timestamp ){
  return upd_aggregate(ptr, slot, timestamp );
}
