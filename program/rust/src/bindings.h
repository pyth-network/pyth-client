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

const size_t PRICE_T_CONF_OFFSET = offsetof(struct pc_price, agg_);