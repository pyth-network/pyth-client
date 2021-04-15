#include "oracle.c"
#include <criterion/criterion.h>

void set_symbol( pc_symbol_t *sptr, const char *sym )
{
  int len = sol_strlen( sym );
  if ( len > PC_SYMBOL_SIZE ) {
    len = PC_SYMBOL_SIZE;
  }
  sol_memset( &sptr->k1_, 0x20, sizeof( pc_symbol_t ) );
  sol_memcpy( &sptr->k1_, sym, len );
}

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
  tptr->num_ = PC_MAP_NODE_SIZE;

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
  tptr->num_ = PC_MAP_NODE_SIZE;
  tptr->magic_ = 0;
  cr_assert( ERROR_INVALID_ARGUMENT == dispatch( &prm, acc ) );
  tptr->magic_ = PC_MAGIC;
  cr_assert( SUCCESS == dispatch( &prm, acc ) );
}

Test(oracle, add_symbol) {
  // start with perfect inputs
  cmd_add_symbol_t idata = {
    .ver_   = PC_VERSION,
    .cmd_   = e_cmd_add_symbol,
    .expo_  = -4,
    .ptype_ = PC_PTYPE_PRICE,
    .sym_   = { .k8_ = { 1UL, 2UL } }
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
  pc_price_t sptr[1];
  sol_memset( sptr, 0, sizeof( pc_price_t ) );
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
      .data_len    = sizeof( pc_price_t ),
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
  cr_assert( sptr->ver_ == PC_VERSION );
  cr_assert( pc_symbol_equal( &idata.sym_, &sptr->sym_ ) );
  cr_assert( sptr->num_ == 0 );
  cr_assert( idata.expo_ == sptr->expo_ );
  cr_assert( mptr->num_ == 1 );
  cr_assert( pc_symbol_equal( &idata.sym_, &mptr->nds_[0].sym_ ) );
  cr_assert( pc_pub_key_equal( (pc_pub_key_t*)&skey,
             &mptr->nds_[0].price_acc_ ) );

  // add same symbol again should map to new key
  acc[2].key = &skey2;
  sol_memset( sptr, 0, sizeof( pc_price_t ) );
  cr_assert( SUCCESS == dispatch( &prm, acc ) );
  cr_assert( mptr->num_ == 1 );
  cr_assert( pc_symbol_equal( &idata.sym_, &mptr->nds_[0].sym_ ) );
  cr_assert( pc_pub_key_equal( (pc_pub_key_t*)&skey2,
             &mptr->nds_[0].price_acc_ ) );
  cr_assert( pc_pub_key_equal( &sptr->next_, (pc_pub_key_t*)&skey ) );

  // invalid parameters
  sol_memset( sptr, 0, sizeof( pc_price_t ) );
  idata.expo_ = 17;
  cr_assert( ERROR_INVALID_ARGUMENT == dispatch( &prm, acc ) );
  idata.expo_ = -17;
  cr_assert( ERROR_INVALID_ARGUMENT == dispatch( &prm, acc ) );
  idata.expo_ = -4;
  idata.sym_.k8_[0] = 0UL;
  idata.sym_.k8_[1] = 0UL;
  cr_assert( ERROR_INVALID_ARGUMENT == dispatch( &prm, acc ) );
  idata.sym_.k8_[0] = 3UL;
  idata.sym_.k8_[1] = 4UL;
  prm.ka_num = 2;
  cr_assert( ERROR_INVALID_ARGUMENT == dispatch( &prm, acc ) );
  prm.ka_num = 3;
  idata.ptype_ = PC_PTYPE_UNKNOWN;
  cr_assert( ERROR_INVALID_ARGUMENT == dispatch( &prm, acc ) );
  idata.ptype_ = PC_PTYPE_PRICE;
  cr_assert( SUCCESS == dispatch( &prm, acc ) );
  cr_assert( mptr->num_ == 2 );

  // test fill up of mapping table
  sol_memset( mptr, 0, sizeof( pc_map_table_t ) );
  mptr->magic_ = PC_MAGIC;
  mptr->ver_ = PC_VERSION;
  for(int i =0;;++i) {
    sol_memset( sptr, 0, sizeof( pc_price_t ) );
    idata.sym_.k8_[0] = i;
    idata.sym_.k8_[1] = i+1;
    uint64_t rc = dispatch( &prm, acc );
    if ( rc != SUCCESS ) {
      cr_assert( i == (PC_MAP_NODE_SIZE) );
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
    .ptype_ = PC_PTYPE_PRICE,
    .sym_   = { .k8_ = { 1UL, 2UL } },
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
  sptr->ptype_ = PC_PTYPE_PRICE;
  pc_symbol_assign( &sptr->sym_, &idata.sym_ );
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
  sptr->ptype_ = PC_PTYPE_PRICE;
  sptr->sym_.k8_[0] = 42;
  // empty symbol
  idata.sym_.k8_[0] = 0UL;
  idata.sym_.k8_[1] = 0UL;
  cr_assert( ERROR_INVALID_ARGUMENT == dispatch( &prm, acc ) );
  idata.sym_.k8_[0] = 42;
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
  cr_assert( sizeof( pc_symbol_t ) == 16 );
  cr_assert( sizeof( pc_map_node_t ) == 56 );
  cr_assert( sizeof( pc_map_table_t ) ==
      sizeof( pc_pub_key_t ) + 12 +
      (PC_MAP_TABLE_SIZE*4) + sizeof( pc_map_node_t )*PC_MAP_NODE_SIZE );
  cr_assert( sizeof( pc_price_info_t ) == 32 );
  cr_assert( sizeof( pc_price_comp_t ) == sizeof( pc_pub_key_t ) +
     2*sizeof( pc_price_info_t ) );
  cr_assert( sizeof( pc_price_t ) == sizeof( pc_symbol_t ) +
      2*sizeof( pc_pub_key_t ) + sizeof( pc_price_info_t ) +
      PC_COMP_SIZE * sizeof( pc_price_comp_t ) + 40 );
}

Test( oracle, upd_price ) {
  cmd_upd_price_t idata = {
    .ver_    = PC_VERSION,
    .cmd_    = e_cmd_upd_price,
    .ptype_  = PC_PTYPE_PRICE,
    .status_ = PC_STATUS_TRADING,
    .sym_    = { .k8_ = { 1UL, 2UL } },
    .price_  = 42L,
    .conf_   = 9L,
    .nonce_  = 100
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
  sptr->num_   = 1;
  pc_pub_key_assign( &sptr->comp_[0].pub_, (pc_pub_key_t*)&pkey );
  pc_symbol_assign( &sptr->sym_, &idata.sym_ );
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
  cr_assert( sptr->curr_slot_ == 1 );

  // add some prices for current slot
  idata.price_ = 43;
  cr_assert( SUCCESS == dispatch( &prm, acc ) );
  cr_assert( sptr->comp_[0].latest_.price_ == 43L );
  cr_assert( sptr->comp_[0].agg_.price_ == 0L );
  idata.price_ = 44;
  cr_assert( SUCCESS == dispatch( &prm, acc ) );
  cr_assert( sptr->comp_[0].latest_.price_ == 44L );
  cr_assert( sptr->comp_[0].agg_.price_ == 0L );
  cr_assert( sptr->comp_[0].latest_.pub_slot_ == 1 );
  cr_assert( sptr->agg_.pub_slot_ == 1 );

  // add next price in new slot triggering snapshot and aggregate calc
  idata.price_ = 81;
  cvar.slot_ = 3;
  cr_assert( SUCCESS == dispatch( &prm, acc ) );
  cr_assert( sptr->comp_[0].latest_.price_ == 81L );
  cr_assert( sptr->comp_[0].agg_.price_ == 44L );
  cr_assert( sptr->comp_[0].latest_.pub_slot_ == 3 );
  cr_assert( sptr->agg_.pub_slot_ == 3 );
  cr_assert( sptr->valid_slot_ == 1 );
  cr_assert( sptr->curr_slot_ == 3 );

  // next price doesnt change but slot does
  cvar.slot_ = 4;
  cr_assert( SUCCESS == dispatch( &prm, acc ) );
  cr_assert( sptr->comp_[0].latest_.price_ == 81L );
  cr_assert( sptr->comp_[0].agg_.price_ == 81L );
  cr_assert( sptr->comp_[0].latest_.pub_slot_ == 4 );
  cr_assert( sptr->agg_.pub_slot_ == 4 );
  cr_assert( sptr->valid_slot_ == 3 );
  cr_assert( sptr->curr_slot_ == 4 );

  // next price doesnt change and neither does aggregate but slot does
  cvar.slot_ = 5;
  cr_assert( SUCCESS == dispatch( &prm, acc ) );
  cr_assert( sptr->comp_[0].latest_.price_ == 81L );
  cr_assert( sptr->comp_[0].agg_.price_ == 81L );
  cr_assert( sptr->comp_[0].latest_.pub_slot_ == 5 );
  cr_assert( sptr->agg_.pub_slot_ == 5 );
  cr_assert( sptr->valid_slot_ == 4 );
  cr_assert( sptr->curr_slot_ == 5 );
}

Test(oracle, upd_aggregate ) {
  pc_pub_key_t pub[1];
  pub->k8_[0] = 11;
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
  px->curr_slot_ = 1000;
  px->comp_[0].latest_ = p1;
  upd_aggregate( px, pub, 1001 );
  cr_assert( px->agg_.price_ == 100 );
  cr_assert( px->agg_.conf_ == 10 );

  // two publishers
  px->num_ = 0;
  px->curr_slot_ = 1000;
  px->comp_[px->num_++].latest_ = p2;
  px->comp_[px->num_++].latest_ = p1;
  upd_aggregate( px, pub, 1001 );
  cr_assert( px->agg_.price_ == 150 );
  cr_assert( px->agg_.conf_ == 15 );

  // three publishers
  px->num_ = 0;
  px->curr_slot_ = 1000;
  px->comp_[px->num_++].latest_ = p2;
  px->comp_[px->num_++].latest_ = p1;
  px->comp_[px->num_++].latest_ = p3;
  upd_aggregate( px, pub, 1001 );
  cr_assert( px->agg_.price_ == 200 );
  cr_assert( px->agg_.conf_ == 20 );

  // four publishers
  px->num_ = 0;
  px->curr_slot_ = 1000;
  px->comp_[px->num_++].latest_ = p3;
  px->comp_[px->num_++].latest_ = p1;
  px->comp_[px->num_++].latest_ = p4;
  px->comp_[px->num_++].latest_ = p2;
  upd_aggregate( px, pub, 1001 );
  cr_assert( px->agg_.price_ == 250 );
  cr_assert( px->agg_.conf_ == 25 );

  // check what happens when nothing publishes for a while
  upd_aggregate( px, pub, 1003 );
  cr_assert( px->agg_.status_ == PC_STATUS_UNKNOWN );
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
    .ptype_ = PC_PTYPE_PRICE,
    .sym_   = { .k8_ = { 1UL, 2UL } },
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
  pc_symbol_assign( &sptr->sym_, &idata.sym_ );
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
