mod pyth_simulator;
mod test_add_price;
mod test_add_product;
mod test_add_publisher;
mod test_aggregation;
mod test_c_code;
mod test_check_valid_signable_account_or_permissioned_funding_account;
mod test_del_price;
mod test_del_product;
mod test_del_publisher;
mod test_ema;
mod test_full_publisher_set;
mod test_init_mapping;
mod test_init_price;
mod test_message;
mod test_permission_migration;
mod test_publish;
mod test_publish_batch;
mod test_set_max_latency;
mod test_set_min_pub;
mod test_sizes;
mod test_upd_aggregate;
mod test_upd_permissions;
mod test_upd_price;
mod test_upd_price_no_fail_on_error;
mod test_upd_product;
mod test_utils;

#[cfg(feature = "pythnet")]
mod test_twap;
#[cfg(feature = "pythnet")]
mod test_upd_price_v2;
