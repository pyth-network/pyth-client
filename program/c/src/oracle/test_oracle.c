char heap_start[8192];
#define PC_HEAP_START (heap_start)
#include "oracle.c"
#include <criterion/criterion.h>

uint64_t MAPPING_ACCOUNT_LAMPORTS = 143821440;
uint64_t PRODUCT_ACCOUNT_LAMPORTS = 4454400;
uint64_t PRICE_ACCOUNT_LAMPORTS = 23942400;

Test( oracle, add_publisher ) {
  // start with perfect inputs
  cmd_add_publisher_t idata = {
    .ver_   = PC_VERSION,
    .cmd_   = e_cmd_add_publisher,
    .pub_   = { .k8_ = { 3UL, 4UL, 5UL, 6UL } }
  };
  SolPubkey p_id  = {.x = { 0xff, }};
  SolPubkey pkey = {.x = { 1, }};
  SolPubkey skey = {.x = { 3, }};
  uint64_t pqty = 100;
  pc_price_t sptr[1];
  sol_memset( sptr, 0, sizeof( pc_price_t ) );
  sptr->magic_ = PC_MAGIC;
  sptr->ver_ = PC_VERSION;
  sptr->type_ = PC_ACCTYPE_PRICE;
  sptr->ptype_ = PC_PTYPE_PRICE;
  SolAccountInfo acc[] = {{
      .key         = &pkey,
      .lamports    = &pqty,
      .data_len    = 0,
      .data        = NULL,
      .owner       = NULL,
      .rent_epoch  = 0,
      .is_signer   = true,
      .is_writable = true,
      .executable  = false
  },{
      .key         = &skey,
      .lamports    = &pqty,
      .data_len    = sizeof( pc_price_t ),
      .data        = (uint8_t*)sptr,
      .owner       = &p_id,
      .rent_epoch  = 0,
      .is_signer   = true,
      .is_writable = true,
      .executable  = false
  } };
  SolParameters prm = {
    .ka         = acc,
    .ka_num     = 2,
    .data       = (const uint8_t*)&idata,
    .data_len   = sizeof( idata ),
    .program_id = &p_id
  };

  // Expect the instruction to fail, because the price account isn't rent exempt
  cr_assert( ERROR_INVALID_ARGUMENT == dispatch( &prm, acc ) );

  // Now give the price account enough lamports to be rent exempt
  acc[1].lamports = &PRICE_ACCOUNT_LAMPORTS;

  cr_assert( SUCCESS == dispatch( &prm, acc ) );
  cr_assert( sptr->num_ == 1 );
  cr_assert( pc_pub_key_equal( &idata.pub_, &sptr->comp_[0].pub_ ) );
  // cant add twice
  cr_assert( ERROR_INVALID_ARGUMENT == dispatch( &prm, acc ) );

  // invalid params
  sol_memset( sptr, 0, sizeof( pc_price_t ) );
  sptr->magic_ = PC_MAGIC;
  sptr->ver_ = PC_VERSION;
  sptr->type_ = PC_ACCTYPE_PRICE;
  // bad price account
  sptr->magic_ = 0;
  cr_assert( ERROR_INVALID_ARGUMENT == dispatch( &prm, acc ) );
  sptr->magic_ = PC_MAGIC;

  // fill up price node
  for( unsigned i = 0;; ++i ) {
    idata.pub_.k8_[0] = 10 + i;
    uint64_t rc = dispatch( &prm, acc );
    if ( rc != SUCCESS ) {
      cr_assert( i == ( unsigned )(PC_COMP_SIZE) );
      break;
    }
    cr_assert( sptr->num_ == i + 1 );
    cr_assert( pc_pub_key_equal( &idata.pub_, &sptr->comp_[i].pub_ ) );
    cr_assert( rc == SUCCESS );
  }
}

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

Test( oracle, upd_price ) {
  cmd_upd_price_t idata = {
    .ver_    = PC_VERSION,
    .cmd_    = e_cmd_upd_price,
    .status_ = PC_STATUS_TRADING,
    .price_  = 42L,
    .conf_   = 2L,
    .pub_slot_ = 1
  };
  SolPubkey p_id  = {.x = { 0xff, }};
  SolPubkey pkey = {.x = { 1, }};
  SolPubkey skey = {.x = { 3, }};
  sysvar_clock_t cvar = {
    .slot_ = 1
  };
  uint64_t pqty = 100, sqty = 200;
  pc_price_t sptr[1];
  sol_memset( sptr, 0, sizeof( pc_price_t ) );
  sptr->magic_ = PC_MAGIC;
  sptr->ver_   = PC_VERSION;
  sptr->ptype_ = PC_PTYPE_PRICE;
  sptr->type_  = PC_ACCTYPE_PRICE;
  sptr->num_   = 1;
  pc_pub_key_assign( &sptr->comp_[0].pub_, (pc_pub_key_t*)&pkey );
  SolAccountInfo acc[] = {{
      .key         = &pkey,
      .lamports    = &pqty,
      .data_len    = 0,
      .data        = NULL,
      .owner       = NULL,
      .rent_epoch  = 0,
      .is_signer   = true,
      .is_writable = true,
      .executable  = false
  },{
      .key         = &skey,
      .lamports    = &PRICE_ACCOUNT_LAMPORTS,
      .data_len    = sizeof( pc_price_t ),
      .data        = (uint8_t*)sptr,
      .owner       = &p_id,
      .rent_epoch  = 0,
      .is_signer   = false,
      .is_writable = true,
      .executable  = false
  },{
      .key         = (SolPubkey*)sysvar_clock,
      .lamports    = &sqty,
      .data_len    = sizeof( sysvar_clock_t ),
      .data        = (uint8_t*)&cvar,
      .owner       = &p_id,
      .rent_epoch  = 0,
      .is_signer   = false,
      .is_writable = false,
      .executable  = false
  }};
  SolParameters prm = {
    .ka         = acc,
    .ka_num     = 3,
    .data       = (const uint8_t*)&idata,
    .data_len   = sizeof( idata ),
    .program_id = &p_id
  };
  cr_assert( SUCCESS == dispatch( &prm, acc ) );
  cr_assert( sptr->comp_[0].latest_.price_ == 42L );
  cr_assert( sptr->comp_[0].latest_.conf_ == 2L );
  cr_assert( sptr->comp_[0].latest_.pub_slot_ == 1 );
  cr_assert( sptr->agg_.pub_slot_ == 1 );
  cr_assert( sptr->valid_slot_ == 0 );

  // add some prices for current slot - get rejected
  idata.price_ = 43;
  cr_assert( ERROR_INVALID_ARGUMENT == dispatch( &prm, acc ) );
  cr_assert( sptr->comp_[0].latest_.price_ == 42L );
  cr_assert( sptr->comp_[0].agg_.price_ == 0 );

  // add next price in new slot triggering snapshot and aggregate calc
  idata.price_ = 81;
  idata.pub_slot_ = 2;
  cvar.slot_ = 3;
  cr_assert( SUCCESSFULLY_UPDATED_AGGREGATE == dispatch( &prm, acc ) );
  cr_assert( sptr->comp_[0].latest_.price_ == 81L );
  cr_assert( sptr->comp_[0].agg_.price_ == 42L );
  cr_assert( sptr->comp_[0].latest_.pub_slot_ == 2 );
  cr_assert( sptr->agg_.pub_slot_ == 3 );
  cr_assert( sptr->valid_slot_ == 1 );

  // next price doesnt change but slot does
  cvar.slot_ = 4;
  idata.pub_slot_ = 3;
  cr_assert( SUCCESSFULLY_UPDATED_AGGREGATE == dispatch( &prm, acc ) );
  cr_assert( sptr->comp_[0].latest_.price_ == 81L );
  cr_assert( sptr->comp_[0].agg_.price_ == 81L );
  cr_assert( sptr->comp_[0].latest_.pub_slot_ == 3 );
  cr_assert( sptr->agg_.pub_slot_ == 4 );
  cr_assert( sptr->valid_slot_ == 3 );

  // next price doesnt change and neither does aggregate but slot does
  idata.pub_slot_ = 4;
  cvar.slot_ = 5;
  cr_assert( SUCCESSFULLY_UPDATED_AGGREGATE == dispatch( &prm, acc ) );
  cr_assert( sptr->comp_[0].latest_.price_ == 81L );
  cr_assert( sptr->comp_[0].agg_.price_ == 81L );
  cr_assert( sptr->comp_[0].latest_.pub_slot_ == 4 );
  cr_assert( sptr->agg_.pub_slot_ == 5 );
  cr_assert( sptr->valid_slot_ == 4 );

  // try to publish back-in-time
  idata.pub_slot_ = 1;
  cr_assert( ERROR_INVALID_ARGUMENT == dispatch( &prm, acc ) );


  // Publishing a wide CI results in a status of unknown.

  // check that someone doesn't accidentally break the test.
  cr_assert(sptr->comp_[0].latest_.status_ == PC_STATUS_TRADING);
  idata.pub_slot_ = 5;
  cvar.slot_ = 6;
  idata.price_ = 50;
  idata.conf_ = 6;
  cr_assert( SUCCESSFULLY_UPDATED_AGGREGATE == dispatch( &prm, acc ) );
  cr_assert( sptr->comp_[0].latest_.price_ == 50L );
  cr_assert( sptr->comp_[0].latest_.conf_ == 6L );
  cr_assert( sptr->comp_[0].latest_.status_ == PC_STATUS_UNKNOWN );
  cr_assert( sptr->comp_[0].latest_.pub_slot_ == 5 );
  cr_assert( sptr->agg_.pub_slot_ == 6 );
  // Aggregate is still trading because it uses price from previous slot
  cr_assert( sptr->agg_.status_ == PC_STATUS_TRADING );

  // Crank one more time and aggregate should be unknown
  idata.pub_slot_ = 6;
  cvar.slot_ = 7;
  cr_assert( SUCCESS == dispatch( &prm, acc ) );
  cr_assert( sptr->agg_.status_ == PC_STATUS_UNKNOWN );
}

Test( oracle, upd_price_no_fail_on_error ) {
  cmd_upd_price_t idata = {
    .ver_    = PC_VERSION,
    .cmd_    = e_cmd_upd_price_no_fail_on_error,
    .status_ = PC_STATUS_TRADING,
    .price_  = 42L,
    .conf_   = 9L,
    .pub_slot_ = 1
  };
  SolPubkey p_id  = {.x = { 0xff, }};
  SolPubkey pkey = {.x = { 1, }};
  SolPubkey skey = {.x = { 3, }};
  sysvar_clock_t cvar = {
    .slot_ = 1
  };
  uint64_t pqty = 100, sqty = 200;
  pc_price_t sptr[1];
  sol_memset( sptr, 0, sizeof( pc_price_t ) );
  sptr->magic_ = PC_MAGIC;
  sptr->ver_   = PC_VERSION;
  sptr->ptype_ = PC_PTYPE_PRICE;
  sptr->type_  = PC_ACCTYPE_PRICE;
  sptr->num_   = 1;

  SolAccountInfo acc[] = {{
      .key         = &pkey,
      .lamports    = &pqty,
      .data_len    = 0,
      .data        = NULL,
      .owner       = NULL,
      .rent_epoch  = 0,
      .is_signer   = true,
      .is_writable = true,
      .executable  = false
  },{
      .key         = &skey,
      .lamports    = &PRICE_ACCOUNT_LAMPORTS,
      .data_len    = sizeof( pc_price_t ),
      .data        = (uint8_t*)sptr,
      .owner       = &p_id,
      .rent_epoch  = 0,
      .is_signer   = false,
      .is_writable = true,
      .executable  = false
  },{
      .key         = (SolPubkey*)sysvar_clock,
      .lamports    = &sqty,
      .data_len    = sizeof( sysvar_clock_t ),
      .data        = (uint8_t*)&cvar,
      .owner       = &p_id,
      .rent_epoch  = 0,
      .is_signer   = false,
      .is_writable = false,
      .executable  = false
  }};
  SolParameters prm = {
    .ka         = acc,
    .ka_num     = 3,
    .data       = (const uint8_t*)&idata,
    .data_len   = sizeof( idata ),
    .program_id = &p_id
  };

  // We haven't permissioned the publish account for the price account
  // yet, so any update should fail silently and have no effect. The
  // transaction should "succeed".
  cr_assert( SUCCESS == dispatch( &prm, acc ) );
  cr_assert( sptr->comp_[0].latest_.price_ == 0L );
  cr_assert( sptr->comp_[0].latest_.conf_ == 0L );
  cr_assert( sptr->comp_[0].latest_.pub_slot_ == 0 );
  cr_assert( sptr->agg_.pub_slot_ == 0 );
  cr_assert( sptr->valid_slot_ == 0 );

  // Now permission the publish account for the price account.
  pc_pub_key_assign( &sptr->comp_[0].pub_, (pc_pub_key_t*)&pkey );

  // The update should now succeed, and have an effect.
  cr_assert( SUCCESS == dispatch( &prm, acc ) );
  cr_assert( sptr->comp_[0].latest_.price_ == 42L );
  cr_assert( sptr->comp_[0].latest_.conf_ == 9L );
  cr_assert( sptr->comp_[0].latest_.pub_slot_ == 1 );
  cr_assert( sptr->agg_.pub_slot_ == 1 );
  cr_assert( sptr->valid_slot_ == 0 );

  // Invalid updates, such as publishing an update for the current slot,
  // should still fail silently and have no effect.
  idata.price_ = 55L;
  idata.conf_ = 22L;
  idata.pub_slot_ = 1;
  cr_assert( SUCCESS == dispatch( &prm, acc ) );
  cr_assert( sptr->comp_[0].latest_.price_ == 42L );
  cr_assert( sptr->comp_[0].latest_.conf_ == 9L );
  cr_assert( sptr->comp_[0].latest_.pub_slot_ == 1 );
  cr_assert( sptr->agg_.pub_slot_ == 1 );
  cr_assert( sptr->valid_slot_ == 0 );
}

Test( oracle, upd_aggregate ) {
  pc_price_t px[1];
  sol_memset( px, 0, sizeof( pc_price_t ) );
  pc_price_info_t p1 = { .price_=100, .conf_=10,
    .status_ = PC_STATUS_TRADING, .corp_act_status_= 0, .pub_slot_ = 1000 };
  pc_price_info_t p2 = { .price_=200, .conf_=20,
    .status_ = PC_STATUS_TRADING, .corp_act_status_= 0, .pub_slot_ = 1000 };
  pc_price_info_t p3 = { .price_=300, .conf_=30,
    .status_ = PC_STATUS_TRADING, .corp_act_status_= 0, .pub_slot_ = 1000 };
  pc_price_info_t p4 = { .price_=400, .conf_=40,
    .status_ = PC_STATUS_TRADING, .corp_act_status_= 0, .pub_slot_ = 1000 };

  // single publisher
  px->num_ = 1;
  px->last_slot_ = 1000;
  px->agg_.pub_slot_ = 1000;
  px->comp_[0].latest_ = p1;
  upd_aggregate( px, 1001, 1 );
  cr_assert( px->agg_.price_ == 100 );
  cr_assert( px->agg_.conf_ == 10 );
  cr_assert( px->twap_.val_ == 100 );
  cr_assert( px->twac_.val_ == 10 );
  cr_assert( px->num_qt_ == 1 );
  cr_assert( px->timestamp_ == 1 );
  cr_assert( px->prev_slot_ == 0 );
  cr_assert( px->prev_price_ == 0 );
  cr_assert( px->prev_conf_ == 0 );
  cr_assert( px->prev_timestamp_ == 0 );

  // two publishers
  px->num_ = 0;
  px->last_slot_ = 1000;
  px->agg_.pub_slot_ = 1000;
  px->comp_[px->num_++].latest_ = p2;
  px->comp_[px->num_++].latest_ = p1;
  upd_aggregate( px, 1001, 2 );
  cr_assert( px->agg_.price_ == 145 );
  cr_assert( px->agg_.conf_ == 55 );
  cr_assert( px->twap_.val_ == 106 );
  cr_assert( px->twac_.val_ == 16 );
  cr_assert( px->num_qt_ == 2 );
  cr_assert( px->timestamp_ == 2 );
  cr_assert( px->prev_slot_ == 1000 );
  cr_assert( px->prev_price_ == 100 );
  cr_assert( px->prev_conf_ == 10 );
  cr_assert( px->prev_timestamp_ == 1 );

  // three publishers
  px->num_ = 0;
  px->last_slot_ = 1000;
  px->agg_.pub_slot_ = 1000;
  px->comp_[px->num_++].latest_ = p2;
  px->comp_[px->num_++].latest_ = p1;
  px->comp_[px->num_++].latest_ = p3;
  upd_aggregate( px, 1001, 3 );
  cr_assert( px->agg_.price_ == 200 );
  cr_assert( px->agg_.conf_ == 90 );
  cr_assert( px->twap_.val_ == 114 );
  cr_assert( px->twac_.val_ == 23 );
  cr_assert( px->num_qt_ == 3 );
  cr_assert( px->timestamp_ == 3 );
  cr_assert( px->prev_slot_ == 1000 );
  cr_assert( px->prev_price_ == 145 );
  cr_assert( px->prev_conf_ == 55 );
  cr_assert( px->prev_timestamp_ == 2 );

  // four publishers
  px->num_ = 0;
  px->last_slot_ = 1000;
  px->agg_.pub_slot_ = 1000;
  px->comp_[px->num_++].latest_ = p3;
  px->comp_[px->num_++].latest_ = p1;
  px->comp_[px->num_++].latest_ = p4;
  px->comp_[px->num_++].latest_ = p2;
  upd_aggregate( px, 1001, 4 );
  cr_assert( px->agg_.price_ == 245 );
  cr_assert( px->agg_.conf_ == 85 );
  cr_assert( px->twap_.val_ == 125 );
  cr_assert( px->twac_.val_ == 28 );
  cr_assert( px->last_slot_ == 1001 );
  cr_assert( px->num_qt_ == 4 );
  cr_assert( px->timestamp_ == 4 );
  cr_assert( px->prev_slot_ == 1000 );
  cr_assert( px->prev_price_ == 200 );
  cr_assert( px->prev_conf_ == 90 );
  cr_assert( px->prev_timestamp_ == 3 );

  upd_aggregate( px, 1025, 5 );
  cr_assert( px->agg_.status_ == PC_STATUS_TRADING );
  cr_assert( px->last_slot_ == 1025 );
  cr_assert( px->num_qt_ == 4 );
  cr_assert( px->timestamp_ == 5 );
  cr_assert( px->prev_slot_ == 1001 );
  cr_assert( px->prev_price_ == 245 );
  cr_assert( px->prev_conf_ == 85 );
  cr_assert( px->prev_timestamp_ == 4 );

  // check what happens when nothing publishes for a while
  upd_aggregate( px, 1026, 10 );
  cr_assert( px->agg_.status_ == PC_STATUS_UNKNOWN );
  cr_assert( px->last_slot_ == 1025 );
  cr_assert( px->num_qt_ == 0 );
  cr_assert( px->timestamp_ == 10 );
  cr_assert( px->prev_slot_ == 1025 );
  cr_assert( px->prev_timestamp_ == 5 );

  // Check that the prev_* fields don't update if the aggregate status is UNKNOWN
  uint64_t prev_slot_ = px->prev_slot_;
  int64_t prev_price_ = px->prev_price_;
  uint64_t prev_conf_ = px->prev_conf_;
  int64_t prev_timestamp_ = px->prev_timestamp_;
  upd_aggregate( px, 1028, 12 );
  cr_assert( px->agg_.status_ == PC_STATUS_UNKNOWN );
  cr_assert( px->timestamp_ == 12 );
  cr_assert( px->prev_slot_ == prev_slot_ );
  cr_assert( px->prev_price_ == prev_price_ );
  cr_assert( px->prev_conf_ == prev_conf_ );
  cr_assert( px->prev_timestamp_ == prev_timestamp_ );
}

Test( oracle, del_publisher ) {
  pc_price_info_t p1 = { .price_=100, .conf_=10,
    .status_ = PC_STATUS_TRADING, .pub_slot_ = 42 };
  pc_price_info_t p2 = { .price_=200, .conf_=20,
    .status_ = PC_STATUS_TRADING, .pub_slot_ = 42 };

  // start with perfect inputs
  pc_pub_key_t p_id = { .k8_ = { 1, 2, 3, 4 } };
  pc_pub_key_t p_id2= { .k8_ = { 2, 3, 4, 5 } };
  pc_pub_key_t p_id3= { .k8_ = { 3, 4, 5, 6 } };
  cmd_add_publisher_t idata = {
    .ver_   = PC_VERSION,
    .cmd_   = e_cmd_del_publisher,
    .pub_   = p_id2
  };
  SolPubkey pkey = {.x = { 1, }};
  SolPubkey skey = {.x = { 3, }};
  uint64_t pqty = 100;
  pc_price_t sptr[1];
  sol_memset( sptr, 0, sizeof( pc_price_t ) );
  sptr->magic_ = PC_MAGIC;
  sptr->ver_ = PC_VERSION;
  sptr->ptype_ = PC_PTYPE_PRICE;
  sptr->type_ = PC_ACCTYPE_PRICE;
  sptr->num_ = 1;
  sptr->comp_[0].latest_ = p1;
  pc_pub_key_assign( &sptr->comp_[0].pub_, (pc_pub_key_t*)&p_id2 );

  SolAccountInfo acc[] = {{
      .key         = &pkey,
      .lamports    = &pqty,
      .data_len    = 0,
      .data        = NULL,
      .owner       = NULL,
      .rent_epoch  = 0,
      .is_signer   = true,
      .is_writable = true,
      .executable  = false
  },{
      .key         = &skey,
      .lamports    = &PRICE_ACCOUNT_LAMPORTS,
      .data_len    = sizeof( pc_price_t ),
      .data        = (uint8_t*)sptr,
      .owner       = (SolPubkey*)&p_id,
      .rent_epoch  = 0,
      .is_signer   = true,
      .is_writable = true,
      .executable  = false
  } };
  SolParameters prm = {
    .ka         = acc,
    .ka_num     = 2,
    .data       = (const uint8_t*)&idata,
    .data_len   = sizeof( idata ),
    .program_id = (SolPubkey*)&p_id
  };
  cr_assert( SUCCESS == dispatch( &prm, acc ) );
  cr_assert( sptr->num_ == 0 );
  cr_assert( sptr->comp_[0].latest_.status_ == 0 );

  sptr->num_ = 2;
  sptr->comp_[0].latest_ = p1;
  sptr->comp_[1].latest_ = p2;
  pc_pub_key_assign( &sptr->comp_[0].pub_, (pc_pub_key_t*)&p_id2 );
  pc_pub_key_assign( &sptr->comp_[1].pub_, (pc_pub_key_t*)&p_id3 );
  cr_assert( SUCCESS == dispatch( &prm, acc ) );
  cr_assert( sptr->num_ == 1 );
  cr_assert( sptr->comp_[0].latest_.price_ == p2.price_ );
  cr_assert( sptr->comp_[1].latest_.status_ == 0 );

  sptr->num_ = 2;
  sptr->comp_[0].latest_ = p1;
  sptr->comp_[1].latest_ = p2;
  pc_pub_key_assign( &sptr->comp_[0].pub_, (pc_pub_key_t*)&p_id3 );
  pc_pub_key_assign( &sptr->comp_[1].pub_, (pc_pub_key_t*)&p_id2 );
  cr_assert( SUCCESS == dispatch( &prm, acc ) );
  cr_assert( sptr->num_ == 1 );
  cr_assert( sptr->comp_[0].latest_.price_ == p1.price_ );
  cr_assert( sptr->comp_[1].latest_.status_ == 0 );
}
