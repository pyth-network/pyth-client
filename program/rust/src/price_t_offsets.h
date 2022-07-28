//The constants below are used by the rust oracle to efficiently access members in price_t
//price_t is a big struct, attemting to deserialize all of it directly in borsh causes stack
//issues on Solana. It is also more efficient to only access the members we need
#include "../../c/src/oracle/oracle.h"
#include <stddef.h>
const size_t PRICE_T_EXPO_OFFSET = offsetof(struct pc_price, expo_);
const size_t PRICE_T_TIMESTAMP_OFFSET = offsetof(struct pc_price, timestamp_);
const size_t PRICE_T_AGGREGATE_OFFSET = offsetof(struct pc_price, agg_);
const size_t PRICE_T_AGGREGATE_CONF_OFFSET = offsetof(struct pc_price, agg_) + offsetof(struct pc_price_info, conf_);
const size_t PRICE_T_AGGREGATE_PRICE_OFFSET = offsetof(struct pc_price, agg_) + offsetof(struct pc_price_info, price_);
const size_t PRICE_T_AGGREGATE_STATUS_OFFSET = offsetof(struct pc_price, agg_) + offsetof(struct pc_price_info, status_);
const size_t PRICE_T_EMA_OFFSET = offsetof(struct pc_price, twap_);
const size_t PRICE_T_PREV_TIMESTAMP_OFFSET = offsetof(struct pc_price, prev_timestamp_);
const size_t PRICE_T_PREV_CONF_OFFSET = offsetof(struct pc_price, prev_conf_);
const size_t PRICE_T_PREV_AGGREGATE_OFFSET = offsetof(struct pc_price, prev_price_);
