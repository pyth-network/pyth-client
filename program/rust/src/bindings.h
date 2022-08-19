#pragma once

#define static_assert _Static_assert

#include <stddef.h>
#include "../../c/src/oracle/oracle.h"

const size_t PC_PRICE_T_COMP_OFFSET = offsetof(struct pc_price, comp_);
const size_t PC_MAP_TABLE_T_PROD_OFFSET = offsetof(struct pc_map_table, prod_);
