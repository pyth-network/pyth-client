char heap_start[8192];
#define PC_HEAP_START (heap_start)
#include "oracle.c"
#include <criterion/criterion.h>

Test(oracle, init_mapping) {

  // start with perfect inputs
  cmd_hdr_t idata = {
    .ver_ = PC_VERSION,
    .cmd_ = e_cmd_init_mapping
  };
  SolPubkey p_id  = {.x = { 0xff, }};
  SolPubkey p_id2 = {.x = { 0xfe, }};
  SolPubkey pkey = {.x = { 1, }};
  SolPubkey mkey = {.x = { 2, }};
  uint64_t pqty = 100, mqty = 200;
  pc_map_table_t mptr[1];
  sol_memset( mptr, 0, sizeof( pc_map_table_t ) );
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
      .key         = &mkey,
      .lamports    = &pqty,
      .data_len    = sizeof( pc_map_table_t ),
      .data        = (uint8_t*)mptr,
      .owner       = &p_id,
      .rent_epoch  = 0,
      .is_signer   = true,
      .is_writable = true,
      .executable  = false
  }};
  SolParameters prm = {
    .ka         = acc,
    .ka_num     = 2,
    .data       = (const uint8_t*)&idata,
    .data_len   = sizeof( idata ),
    .program_id = &p_id
  };
  cr_assert( SUCCESS == dispatch( &prm, acc ) );
  cr_assert( mptr->ver_ == PC_VERSION );
  cr_assert( mptr->type_ == PC_ACCTYPE_MAPPING );

  // cant reinitialize because version has been set
  cr_assert( ERROR_INVALID_ARGUMENT == dispatch( &prm, acc ) );
  sol_memset( mptr, 0, sizeof( pc_map_table_t ) );

  // check other params
  prm.ka_num = 1;
  cr_assert( ERROR_INVALID_ARGUMENT == dispatch( &prm, acc ) );
  prm.ka_num = 2;
  acc[0].is_signer = false;
  cr_assert( ERROR_INVALID_ARGUMENT == dispatch( &prm, acc ) );
  acc[0].is_signer = true;
  acc[1].is_signer = false;
  cr_assert( ERROR_INVALID_ARGUMENT == dispatch( &prm, acc ) );
  acc[1].is_signer = true;
  acc[0].is_writable = false;
  cr_assert( ERROR_INVALID_ARGUMENT == dispatch( &prm, acc ) );
  acc[0].is_writable = true;
  acc[1].is_writable = false;
  cr_assert( ERROR_INVALID_ARGUMENT == dispatch( &prm, acc ) );
  acc[1].is_writable = true;
  acc[1].owner = &p_id2;
  cr_assert( ERROR_INVALID_ARGUMENT == dispatch( &prm, acc ) );
  acc[1].owner = &p_id;
  acc[1].data_len = 0;
  cr_assert( ERROR_INVALID_ARGUMENT == dispatch( &prm, acc ) );
  acc[1].data_len = sizeof( pc_map_table_t );
  cr_assert( SUCCESS == dispatch( &prm, acc ) );
}

Test(oracle, add_mapping ) {
  // start with perfect inputs
  cmd_hdr_t idata = {
    .ver_ = PC_VERSION,
    .cmd_ = e_cmd_add_mapping
  };
  pc_map_table_t tptr[1];
  sol_memset( tptr, 0, sizeof( pc_map_table_t ) );
  tptr->magic_ = PC_MAGIC;
  tptr->ver_ = PC_VERSION;
  tptr->num_ = PC_MAP_TABLE_SIZE;
  tptr->type_ = PC_ACCTYPE_MAPPING;

  SolPubkey p_id  = {.x = { 0xff, }};
  SolPubkey pkey = {.x = { 1, }};
  SolPubkey tkey = {.x = { 2, }};
  SolPubkey mkey = {.x = { 3, }};
  uint64_t pqty = 100, tqty = 100, mqty = 200;
  pc_map_table_t mptr[1];
  sol_memset( mptr, 0, sizeof( pc_map_table_t ) );
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
      .key         = &tkey,
      .lamports    = &pqty,
      .data_len    = sizeof( pc_map_table_t ),
      .data        = (uint8_t*)tptr,
      .owner       = &p_id,
      .rent_epoch  = 0,
      .is_signer   = true,
      .is_writable = true,
      .executable  = false
  },{
      .key         = &mkey,
      .lamports    = &tqty,
      .data_len    = sizeof( pc_map_table_t ),
      .data        = (uint8_t*)mptr,
      .owner       = &p_id,
      .rent_epoch  = 0,
      .is_signer   = true,
      .is_writable = true,
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
  cr_assert( mptr->magic_ == PC_MAGIC );
  cr_assert( mptr->ver_ == PC_VERSION );
  cr_assert( pc_pub_key_equal( &tptr->next_, (pc_pub_key_t*)&mkey ) );
  cr_assert( pc_pub_key_is_zero( &mptr->next_ ) );
  pc_pub_key_set_zero( &tptr->next_ );
  sol_memset( mptr, 0, sizeof( pc_map_table_t ) );
  tptr->num_ = 0;
  cr_assert( ERROR_INVALID_ARGUMENT == dispatch( &prm, acc ) );
  cr_assert( pc_pub_key_is_zero( &tptr->next_ ) );
  tptr->num_ = PC_MAP_TABLE_SIZE;
  tptr->magic_ = 0;
  cr_assert( ERROR_INVALID_ARGUMENT == dispatch( &prm, acc ) );
  tptr->magic_ = PC_MAGIC;
  cr_assert( SUCCESS == dispatch( &prm, acc ) );
}

Test(oracle, add_product) {
  // start with perfect inputs
  cmd_add_product_t idata = {
    .ver_   = PC_VERSION,
    .cmd_   = e_cmd_add_product,
  };
  SolPubkey p_id  = {.x = { 0xff, }};
  SolPubkey p_id2 = {.x = { 0xfe, }};
  SolPubkey pkey = {.x = { 1, }};
  SolPubkey mkey = {.x = { 2, }};
  SolPubkey skey = {.x = { 3, }};
  SolPubkey skey2 = {.x = { 4, }};
  uint64_t pqty = 100, mqty = 200;
  pc_map_table_t mptr[1];
  sol_memset( mptr, 0, sizeof( pc_map_table_t ) );
  mptr->magic_ = PC_MAGIC;
  mptr->ver_ = PC_VERSION;
  mptr->type_ = PC_ACCTYPE_MAPPING;
  char sdata[PC_PROD_ACC_SIZE];
  pc_prod_t *sptr = (pc_prod_t*)sdata;
  sol_memset( sptr, 0, PC_PROD_ACC_SIZE );
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
      .key         = &mkey,
      .lamports    = &pqty,
      .data_len    = sizeof( pc_map_table_t ),
      .data        = (uint8_t*)mptr,
      .owner       = &p_id,
      .rent_epoch  = 0,
      .is_signer   = true,
      .is_writable = true,
      .executable  = false
  },{
      .key         = &skey,
      .lamports    = &pqty,
      .data_len    = PC_PROD_ACC_SIZE,
      .data        = (uint8_t*)sptr,
      .owner       = &p_id,
      .rent_epoch  = 0,
      .is_signer   = true,
      .is_writable = true,
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
  cr_assert( sptr->magic_ == PC_MAGIC );
  cr_assert( sptr->ver_ == PC_VERSION );
  cr_assert( sptr->type_ == PC_ACCTYPE_PRODUCT );
  cr_assert( sptr->size_ == sizeof( pc_prod_t ) );
  cr_assert( mptr->num_ == 1 );
  cr_assert( pc_pub_key_equal( (pc_pub_key_t*)&skey, &mptr->prod_[0] ) );

  // 2nd product
  acc[2].key = &skey2;
  sol_memset( sptr, 0, PC_PROD_ACC_SIZE );
  cr_assert( SUCCESS == dispatch( &prm, acc ) );
  cr_assert( mptr->num_ == 2 );
  cr_assert( pc_pub_key_equal( &mptr->prod_[1], (pc_pub_key_t*)&skey2 ) );

  // invalid acc size
  acc[2].data_len = 1;
  cr_assert(  ERROR_INVALID_ARGUMENT== dispatch( &prm, acc ) );
  acc[2].data_len = PC_PROD_ACC_SIZE;

  // test fill up of mapping table
  sol_memset( mptr, 0, sizeof( pc_map_table_t ) );
  mptr->magic_ = PC_MAGIC;
  mptr->ver_ = PC_VERSION;
  mptr->type_ = PC_ACCTYPE_MAPPING;
  for(int i =0;;++i) {
    sol_memset( sptr, 0, PC_PROD_ACC_SIZE );
    uint64_t rc = dispatch( &prm, acc );
    if ( rc != SUCCESS ) {
      cr_assert( i == (PC_MAP_TABLE_SIZE) );
      break;
    }
    cr_assert( mptr->num_ == i+1 );
    cr_assert( rc == SUCCESS );
  }
}

Test( oracle, add_publisher ) {
  // start with perfect inputs
  cmd_add_publisher_t idata = {
    .ver_   = PC_VERSION,
    .cmd_   = e_cmd_add_publisher,
    .pub_   = { .k8_ = { 3UL, 4UL, 5UL, 6UL } }
  };
  SolPubkey p_id  = {.x = { 0xff, }};
  SolPubkey p_id2 = {.x = { 0xfe, }};
  SolPubkey pkey = {.x = { 1, }};
  SolPubkey skey = {.x = { 3, }};
  uint64_t pqty = 100, sqty = 200;
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
      .lamports    = &sqty,
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
  for(int i =0;;++i) {
    idata.pub_.k8_[0] = 10+i;
    uint64_t rc = dispatch( &prm, acc );
    if ( rc != SUCCESS ) {
      cr_assert( i == (PC_COMP_SIZE) );
      break;
    }
    cr_assert( sptr->num_ == i+1 );
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
    .conf_   = 9L,
    .pub_slot_ = 1
  };
  SolPubkey p_id  = {.x = { 0xff, }};
  SolPubkey p_id2 = {.x = { 0xfe, }};
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
      .lamports    = &sqty,
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
  cr_assert( sptr->comp_[0].latest_.conf_ == 9L );
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
  cr_assert( SUCCESS == dispatch( &prm, acc ) );
  cr_assert( sptr->comp_[0].latest_.price_ == 81L );
  cr_assert( sptr->comp_[0].agg_.price_ == 42L );
  cr_assert( sptr->comp_[0].latest_.pub_slot_ == 2 );
  cr_assert( sptr->agg_.pub_slot_ == 3 );
  cr_assert( sptr->valid_slot_ == 1 );

  // next price doesnt change but slot does
  cvar.slot_ = 4;
  idata.pub_slot_ = 3;
  cr_assert( SUCCESS == dispatch( &prm, acc ) );
  cr_assert( sptr->comp_[0].latest_.price_ == 81L );
  cr_assert( sptr->comp_[0].agg_.price_ == 81L );
  cr_assert( sptr->comp_[0].latest_.pub_slot_ == 3 );
  cr_assert( sptr->agg_.pub_slot_ == 4 );
  cr_assert( sptr->valid_slot_ == 3 );

  // next price doesnt change and neither does aggregate but slot does
  idata.pub_slot_ = 4;
  cvar.slot_ = 5;
  cr_assert( SUCCESS == dispatch( &prm, acc ) );
  cr_assert( sptr->comp_[0].latest_.price_ == 81L );
  cr_assert( sptr->comp_[0].agg_.price_ == 81L );
  cr_assert( sptr->comp_[0].latest_.pub_slot_ == 4 );
  cr_assert( sptr->agg_.pub_slot_ == 5 );
  cr_assert( sptr->valid_slot_ == 4 );

  // try to publish back-in-time
  idata.pub_slot_ = 1;
  cr_assert( ERROR_INVALID_ARGUMENT == dispatch( &prm, acc ) );
}

Test( oracle, upd_aggregate ) {
  pc_price_t px[1];
  sol_memset( px, 0, sizeof( pc_price_t ) );
  pc_price_info_t p1 = { .price_=100, .conf_=10,
    .status_ = PC_STATUS_TRADING, .pub_slot_ = 1000 };
  pc_price_info_t p2 = { .price_=200, .conf_=20,
    .status_ = PC_STATUS_TRADING, .pub_slot_ = 1000 };
  pc_price_info_t p3 = { .price_=300, .conf_=30,
    .status_ = PC_STATUS_TRADING, .pub_slot_ = 1000 };
  pc_price_info_t p4 = { .price_=400, .conf_=40,
    .status_ = PC_STATUS_TRADING, .pub_slot_ = 1000 };

  // single publisher
  px->num_ = 1;
  px->last_slot_ = 1000;
  px->agg_.pub_slot_ = 1000;
  px->comp_[0].latest_ = p1;
  upd_aggregate( px, 1001 );
  cr_assert( px->agg_.price_ == 100 );
  cr_assert( px->agg_.conf_ == 50 );
  cr_assert( px->twap_.val_ == 100 );
  cr_assert( px->twac_.val_ == 50 );

  // two publishers
  px->num_ = 0;
  px->last_slot_ = 1000;
  px->agg_.pub_slot_ = 1000;
  px->comp_[px->num_++].latest_ = p2;
  px->comp_[px->num_++].latest_ = p1;
  upd_aggregate( px, 1001 );
  cr_assert( px->agg_.price_ == 147 );
  cr_assert( px->agg_.conf_ == 48 );
  cr_assert( px->twap_.val_ == 123 );
  cr_assert( px->twac_.val_ == 48 );

  // three publishers
  px->num_ = 0;
  px->last_slot_ = 1000;
  px->agg_.pub_slot_ = 1000;
  px->comp_[px->num_++].latest_ = p2;
  px->comp_[px->num_++].latest_ = p1;
  px->comp_[px->num_++].latest_ = p3;
  upd_aggregate( px, 1001 );
  cr_assert( px->agg_.price_ == 191 );
  cr_assert( px->agg_.conf_ == 74 );
  cr_assert( px->twap_.val_ == 146 );
  cr_assert( px->twac_.val_ == 57 );

  // four publishers
  px->num_ = 0;
  px->last_slot_ = 1000;
  px->agg_.pub_slot_ = 1000;
  px->comp_[px->num_++].latest_ = p3;
  px->comp_[px->num_++].latest_ = p1;
  px->comp_[px->num_++].latest_ = p4;
  px->comp_[px->num_++].latest_ = p2;
  upd_aggregate( px, 1001 );
  cr_assert( px->agg_.price_ == 235 );
  cr_assert( px->agg_.conf_ == 99 );
  cr_assert( px->twap_.val_ == 168 );
  cr_assert( px->twac_.val_ == 67 );
  cr_assert( px->last_slot_ == 1001 );

  upd_aggregate( px, 1025 );
  cr_assert( px->agg_.status_ == PC_STATUS_TRADING );
  cr_assert( px->last_slot_ == 1025 );

  // check what happens when nothing publishes for a while
  upd_aggregate( px, 1026 );
  cr_assert( px->agg_.status_ == PC_STATUS_UNKNOWN );
  cr_assert( px->last_slot_ == 1025 );
}

Test( oracle, del_publisher ) {
  pc_price_info_t p1 = { .price_=100, .conf_=10,
    .status_ = PC_STATUS_TRADING, .pub_slot_ = 42 };
  pc_price_info_t p2 = { .price_=200, .conf_=20,
    .status_ = PC_STATUS_TRADING, .pub_slot_ = 42 };
  pc_price_info_t p3 = { .price_=300, .conf_=30,
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
  uint64_t pqty = 100, sqty = 200;
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
      .lamports    = &sqty,
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
