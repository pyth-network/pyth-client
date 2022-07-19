//The constans belw are used by the rust oracle to efficiently access members
#include "../../c/src/oracle/oracle.h"
#include <stddef.h>
const size_t PRICE_T_EXPO_OFFSET = offsetof(struct pc_price, expo_);
const size_t PRICE_T_TIMESTAMP_OFFSET = offsetof(struct pc_price, timestamp_);
const size_t PRICE_T_CONF_OFFSET = offsetof(struct pc_price, agg_) + offsetof(struct pc_price_info, conf_);
const size_t PRICE_T_AGGREGATE_OFFSET = offsetof(struct pc_price, agg_) + offsetof(struct pc_price_info, price_);
const size_t PRICE_T_AGGREGATE_STATUS = offsetof(struct pc_price, agg_) + offsetof(struct pc_price_info, status_);
const size_t PRICE_T_PREV_TIMESTAMP_OFFSET = offsetof(struct pc_price, prev_timestamp_);
const size_t PRICE_T_PREV_CONF_OFFSET = offsetof(struct pc_price, prev_conf_);
const size_t PRICE_T_PREV_AGGREGATE_OFFSET = offsetof(struct pc_price, prev_price_);