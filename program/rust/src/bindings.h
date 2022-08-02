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

#include "../../c/src/oracle/oracle.h"
#include "price_t_offsets.h"

// Store #define constants as values so they are accessible from rust.
const uint32_t PC_MAGIC_V = PC_MAGIC;
const uint32_t PC_VERSION_V = PC_VERSION;

const uint32_t PC_ACCTYPE_MAPPING_V = PC_ACCTYPE_MAPPING;
const uint32_t PC_ACCTYPE_PRODUCT_V = PC_ACCTYPE_PRODUCT;
const uint32_t PC_ACCTYPE_PRICE_V = PC_ACCTYPE_PRICE;
const uint32_t PC_ACCTYPE_TEST_V = PC_ACCTYPE_TEST;
