#pragma once

#define static_assert _Static_assert

typedef signed char int8_t;
typedef unsigned char uint8_t;
typedef signed short int16_t;
typedef unsigned short uint16_t;
typedef signed int int32_t;
typedef unsigned int uint32_t;
typedef signed long int int64_t;
typedef unsigned long int uint64_t;

#include <stddef.h>
#include "../../c/src/oracle/oracle.h"

const size_t PC_PRICE_T_COMP_OFFSET = offsetof(struct pc_price, comp_);
const size_t PC_MAP_TABLE_T_PROD_OFFSET = offsetof(struct pc_map_table, prod_);
