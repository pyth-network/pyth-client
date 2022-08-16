char heap_start[8192];
#define PC_HEAP_START (heap_start)
#include "oracle.c"
#include <criterion/criterion.h>

uint64_t MAPPING_ACCOUNT_LAMPORTS = 143821440;
uint64_t PRODUCT_ACCOUNT_LAMPORTS = 4454400;
uint64_t PRICE_ACCOUNT_LAMPORTS = 23942400;

Test(oracle, pc_size ) {
  cr_assert( sizeof( pc_pub_key_t ) == 32 );
  cr_assert( sizeof( pc_map_table_t ) ==
      24 + (PC_MAP_TABLE_SIZE+1)*sizeof( pc_pub_key_t) );
  cr_assert( sizeof( pc_price_info_t ) == 32 );
  cr_assert( sizeof( pc_price_comp_t ) == sizeof( pc_pub_key_t ) +
     2*sizeof( pc_price_info_t ) );
  cr_assert( sizeof( pc_price_t ) == 48 +
      8*sizeof(int64_t) +
      3*sizeof( pc_pub_key_t ) + sizeof( pc_price_info_t ) +
      PC_COMP_SIZE * sizeof( pc_price_comp_t ) );
}
