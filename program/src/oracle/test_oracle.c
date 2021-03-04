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

Test(oracle, map_table ) {
  pc_pub_key_t p1[1],p2[1];
  p1->k8_[0] = 1UL;
  p1->k8_[1] = 2UL;
  p1->k8_[2] = 3UL;
  p1->k8_[3] = 4UL;
  p2->k8_[0] = 2UL;
  p2->k8_[1] = 3UL;
  p2->k8_[2] = 4UL;
  p2->k8_[3] = 5UL;
  pc_symbol_t s1[1], s2[1], s3[1];
  set_symbol( s1, "us.eq.SYMBOL1" );
  set_symbol( s2, "eu.eq.FOO" );
  set_symbol( s3, "as.fi.TEST" );

  pc_map_table_t tab[1];
  sol_memset( tab, 0, sizeof( tab ) );

  cr_assert( PC_RETURN_SUCCESS ==
     pc_map_table_add_symbol( tab, s1, p1 ) );
  cr_assert( PC_RETURN_SYMBOL_EXISTS ==
     pc_map_table_add_symbol( tab, s1, p2 ) );
  cr_assert( PC_RETURN_SUCCESS ==
     pc_map_table_add_symbol( tab, s2, p2 ) );

  // get symbol/account
  pc_pub_key_t *res = NULL;
  res = pc_map_table_get_account( tab, s1 );
  cr_assert( res!= NULL && pc_pub_key_equal( res, p1 ) );
  res = pc_map_table_get_account( tab, s2 );
  cr_assert( res!=NULL && pc_pub_key_equal( res, p2 ) );
  res = pc_map_table_get_account( tab, s3 );
  cr_assert( res == NULL );
}

Test(oracle, map_table_capacity ) {
  // test max capacity
  pc_map_table_t tab[1];
  sol_memset( tab, 0, sizeof( tab ) );

  // max capacity through adding symbols
  for(int i =0;;++i) {
    pc_pub_key_t acc[1];
    acc->k8_[0] = i;
    acc->k8_[1] = i+1;
    acc->k8_[2] = i+2;
    acc->k8_[3] = i+3;
    pc_symbol_t sym[1];
    sym->k8_[0] = i;
    sym->k8_[1] = i+1;
    uint64_t rc = pc_map_table_add_symbol( tab, sym, acc );
    if ( rc == PC_RETURN_MAX_CAPACITY ) {
      cr_assert( i == (PC_MAP_NODE_SIZE) );
      cr_assert( PC_RETURN_MAX_CAPACITY ==
          pc_map_table_add_symbol( tab, sym, acc ) );
      break;
    }
    cr_assert( rc == PC_RETURN_SUCCESS );
  }
}

Test(oracle, price_node ) {

  pc_price_node_t nd[1];
  sol_memset( nd, 0, sizeof( nd ) );
  nd->ver_  = 1;
  nd->expo_ = -4;
  set_symbol( &nd->sym_, "symbol1" );
  pc_pub_key_t p1[1],p2[1];
  p1->k8_[0] = 4UL;
  p1->k8_[1] = 3UL;
  p1->k8_[2] = 2UL;
  p1->k8_[3] = 1UL;
  p2->k8_[0] = 5UL;
  p2->k8_[1] = 4UL;
  p2->k8_[2] = 3UL;
  p2->k8_[3] = 2UL;

  // try adding publishers
  cr_assert( PC_RETURN_SUCCESS == pc_price_node_add_publisher(
        nd, p1 ) );
  cr_assert( 1 == nd->num_ );
  cr_assert( PC_RETURN_PUBKEY_EXISTS ==
      pc_price_node_add_publisher( nd, p1 ) );
  cr_assert( PC_RETURN_SUCCESS ==
      pc_price_node_add_publisher( nd, p2 ) );
  cr_assert( PC_RETURN_PUBKEY_EXISTS ==
    pc_price_node_add_publisher( nd, p1 ) );
  cr_assert( PC_RETURN_PUBKEY_EXISTS ==
    pc_price_node_add_publisher( nd, p2 ));
  cr_assert( 2 == nd->num_ );
  for( unsigned i=0; i != PC_COMP_SIZE; ++i ) {
    pc_pub_key_t pi[1];
    pi->k8_[0] = i;
    pi->k8_[1] = i+1;
    pi->k8_[2] = i+2;
    pi->k8_[3] = i+3;
    uint64_t res = pc_price_node_add_publisher( nd, pi );
    cr_assert( res == PC_RETURN_SUCCESS || res == PC_RETURN_MAX_CAPACITY );
    if ( res == PC_RETURN_MAX_CAPACITY ) {
      cr_assert( nd->num_ == PC_COMP_SIZE );
    } else {
      cr_assert( PC_RETURN_PUBKEY_EXISTS ==
          pc_price_node_add_publisher( nd, p1 ) );
    }
    cr_assert( nd->comp_[i].price_ == PC_INVALID_PRICE );
  }

  // try deleting publishers
  cr_assert( PC_RETURN_SUCCESS == pc_price_node_del_publisher( nd, p1 ) );
  cr_assert( PC_COMP_SIZE-1 == nd->num_ );
  cr_assert( pc_pub_key_is_zero( &nd->comp_[PC_COMP_SIZE-1].pub_ ) );
  cr_assert( pc_pub_key_equal( p2, &nd->comp_[0].pub_ ) );
  cr_assert( nd->comp_[PC_COMP_SIZE-1].price_ == 0L );
}

Test(oracle, init_mapping) {

  // start with perfect inputs
  uint32_t idata = e_cmd_init_mapping;
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
  cr_assert( SUCCESS == init_mapping( &prm, acc ) );
  cr_assert( mptr->ver_ == PC_VERSION );

  // cant reinitialize because version has been set
  cr_assert( ERROR_INVALID_ARGUMENT == init_mapping( &prm, acc ) );
  mptr->ver_ = 0;

  // check other params
  prm.ka_num = 1;
  cr_assert( ERROR_INVALID_ARGUMENT == init_mapping( &prm, acc ) );
  prm.ka_num = 2;
  acc[0].is_signer = false;
  cr_assert( ERROR_INVALID_ARGUMENT == init_mapping( &prm, acc ) );
  acc[0].is_signer = true;
  acc[1].is_signer = false;
  cr_assert( ERROR_INVALID_ARGUMENT == init_mapping( &prm, acc ) );
  acc[1].is_signer = true;
  acc[0].is_writable = false;
  cr_assert( ERROR_INVALID_ARGUMENT == init_mapping( &prm, acc ) );
  acc[0].is_writable = true;
  acc[1].is_writable = false;
  cr_assert( ERROR_INVALID_ARGUMENT == init_mapping( &prm, acc ) );
  acc[1].is_writable = true;
  acc[1].owner = &p_id2;
  cr_assert( ERROR_INVALID_ARGUMENT == init_mapping( &prm, acc ) );
  acc[1].owner = &p_id;
  acc[1].data_len = 0;
  cr_assert( ERROR_INVALID_ARGUMENT == init_mapping( &prm, acc ) );
  acc[1].data_len = sizeof( pc_map_table_t );
  cr_assert( SUCCESS == init_mapping( &prm, acc ) );
}

Test(oracle, pc_size ) {
  cr_assert( sizeof( pc_pub_key_t ) == 32 );
  cr_assert( sizeof( pc_symbol_t ) == 16 );
  cr_assert( sizeof( pc_map_node_t ) == 56 );
  cr_assert( sizeof( pc_map_table_t ) ==
      2*sizeof( pc_pub_key_t ) + 12 +
      (PC_MAP_TABLE_SIZE*4) + sizeof( pc_map_node_t )*PC_MAP_NODE_SIZE );
  cr_assert( sizeof( pc_price_node_t ) ==
      (1+PC_COMP_SIZE)*sizeof(pc_price_t)+sizeof( pc_symbol_t ) + 16 );
  cr_assert( sizeof( pc_price_t ) == 32+16 );
}

Test(oracle, add_symbol) {
  // start with perfect inputs
  cmd_add_symbol_t idata = {
    .cmd_  = e_cmd_add_symbol,
    .expo_ = -4,
    .sym_  = { .k8_ = { 1UL, 2UL } }
  };
  SolPubkey p_id  = {.x = { 0xff, }};
  SolPubkey p_id2 = {.x = { 0xfe, }};
  SolPubkey pkey = {.x = { 1, }};
  SolPubkey mkey = {.x = { 2, }};
  SolPubkey skey = {.x = { 3, }};
  uint64_t pqty = 100, mqty = 200;
  pc_map_table_t mptr[1];
  sol_memset( mptr, 0, sizeof( pc_map_table_t ) );
  mptr->ver_ = 1;
  pc_price_node_t sptr[1];
  sol_memset( sptr, 0, sizeof( pc_price_node_t ) );
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
      .data_len    = sizeof( pc_price_node_t ),
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
  cr_assert( SUCCESS == add_symbol( &prm, acc ) );
  cr_assert( sptr->ver_ == PC_VERSION );
  cr_assert( pc_symbol_equal( &idata.sym_, &sptr->sym_ ) );
  cr_assert( sptr->num_ == 0 );
  cr_assert( idata.expo_ == sptr->expo_ );
  cr_assert( mptr->num_ == 1 );
  cr_assert( pc_symbol_equal( &idata.sym_, &mptr->nds_[0].sym_ ) );
  cr_assert( pc_pub_key_equal( (pc_pub_key_t*)&skey, &mptr->nds_[0].acc_ ) );

  // add same symbol again
  sol_memset( sptr, 0, sizeof( pc_price_node_t ) );
  cr_assert( PC_RETURN_SYMBOL_EXISTS == add_symbol( &prm, acc ) );
  cr_assert( PC_RETURN_SYMBOL_EXISTS == add_symbol( &prm, acc ) );

  // invalid parameters
  idata.expo_ = 17;
  cr_assert( ERROR_INVALID_ARGUMENT == add_symbol( &prm, acc ) );
  idata.expo_ = -17;
  cr_assert( ERROR_INVALID_ARGUMENT == add_symbol( &prm, acc ) );
  idata.expo_ = -4;
  idata.sym_.k8_[0] = 0UL;
  idata.sym_.k8_[1] = 0UL;
  cr_assert( ERROR_INVALID_ARGUMENT == add_symbol( &prm, acc ) );
  idata.sym_.k8_[0] = 3UL;
  idata.sym_.k8_[1] = 4UL;
  prm.ka_num = 2;
  cr_assert( ERROR_INVALID_ARGUMENT == add_symbol( &prm, acc ) );
  prm.ka_num = 3;
  cr_assert( SUCCESS == add_symbol( &prm, acc ) );
  cr_assert( mptr->num_ == 2 );

  // invalid mapping
  idata.sym_.k8_[0] = 5UL;
  sol_memset( sptr, 0, sizeof( pc_price_node_t ) );
  mptr->ver_ = 0;
  cr_assert( ERROR_INVALID_ARGUMENT == add_symbol( &prm, acc ) );
  mptr->ver_ = 1000;
  cr_assert( ERROR_INVALID_ARGUMENT == add_symbol( &prm, acc ) );
  mptr->ver_ = 0;

  // invalid symbol data
  sptr->ver_ = 1;
  cr_assert( ERROR_INVALID_ARGUMENT == add_symbol( &prm, acc ) );
  sptr->ver_ = 0;
  sptr->num_ = 1;
  cr_assert( ERROR_INVALID_ARGUMENT == add_symbol( &prm, acc ) );
  sptr->num_ = 0;
}

Test( oracle, add_publisher ) {
  // start with perfect inputs
  cmd_add_publisher_t idata = {
    .cmd_  = e_cmd_add_publisher,
    .pad_  = 0,
    .sym_  = { .k8_ = { 1UL, 2UL } },
    .pub_  = { .k8_ = { 3UL, 4UL, 5UL, 6UL } }
  };
  SolPubkey p_id  = {.x = { 0xff, }};
  SolPubkey p_id2 = {.x = { 0xfe, }};
  SolPubkey pkey = {.x = { 1, }};
  SolPubkey skey = {.x = { 3, }};
  uint64_t pqty = 100, sqty = 200;
  pc_price_node_t sptr[1];
  sol_memset( sptr, 0, sizeof( pc_price_node_t ) );
  sptr->ver_ = PC_VERSION;
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
      .data_len    = sizeof( pc_price_node_t ),
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
  cr_assert( SUCCESS == add_publisher( &prm, acc ) );
  cr_assert( sptr->num_ == 1 );
  cr_assert( pc_pub_key_equal( &idata.pub_, &sptr->comp_[0].pub_ ) );
  cr_assert( sptr->comp_[0].price_ == PC_INVALID_PRICE );
  cr_assert( PC_RETURN_PUBKEY_EXISTS == add_publisher( &prm, acc ) );
  idata.sym_.k8_[0] = 0UL;
  cr_assert( ERROR_INVALID_ARGUMENT == add_publisher( &prm, acc ) );
  idata.sym_.k8_[1] = 0UL;
  cr_assert( ERROR_INVALID_ARGUMENT == add_publisher( &prm, acc ) );
  pc_symbol_assign( &idata.sym_, &sptr->sym_ );
  cr_assert( PC_RETURN_PUBKEY_EXISTS == add_publisher( &prm, acc ) );
  sptr->ver_ = 0;
  cr_assert( ERROR_INVALID_ARGUMENT == add_publisher( &prm, acc ) );
  sptr->ver_ = PC_VERSION;
  acc[1].data_len = 1;
  cr_assert( ERROR_INVALID_ARGUMENT == add_publisher( &prm, acc ) );
  acc[1].data_len = sizeof( pc_price_node_t );
  cr_assert( PC_RETURN_PUBKEY_EXISTS == add_publisher( &prm, acc ) );
  pc_pub_key_set_zero( &idata.pub_ );
  cr_assert( ERROR_INVALID_ARGUMENT == add_publisher( &prm, acc ) );
}

Test( oracle, upd_price ) {
  cmd_upd_price_t idata = {
    .cmd_   = e_cmd_upd_price,
    .pad_   = 0,
    .sym_   = { .k8_ = { 1UL, 2UL } },
    .price_ = 42L,
    .conf_  = 9L
  };
  SolPubkey p_id  = {.x = { 0xff, }};
  SolPubkey p_id2 = {.x = { 0xfe, }};
  SolPubkey pkey = {.x = { 1, }};
  SolPubkey skey = {.x = { 3, }};
  uint64_t pqty = 100, sqty = 200;
  pc_price_node_t sptr[1];
  sol_memset( sptr, 0, sizeof( pc_price_node_t ) );
  sptr->ver_ = PC_VERSION;
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
      .data_len    = sizeof( pc_price_node_t ),
      .data        = (uint8_t*)sptr,
      .owner       = &p_id,
      .rent_epoch  = 0,
      .is_signer   = false,
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
  cr_assert( SUCCESS == pc_price_node_add_publisher(
        sptr, (pc_pub_key_t*)&pkey ) );
  cr_assert( SUCCESS == upd_price( &prm, acc ) );
  cr_assert( sptr->comp_[0].price_ == idata.price_ );
  cr_assert( sptr->comp_[0].conf_ == idata.conf_ );
  cr_assert( SUCCESS == upd_price( &prm, acc ) );
  sptr->ver_ = 0;
  cr_assert( ERROR_INVALID_ARGUMENT == upd_price( &prm, acc ) );
  sptr->ver_ = PC_VERSION+1;
  cr_assert( ERROR_INVALID_ARGUMENT == upd_price( &prm, acc ) );
  sptr->ver_ = PC_VERSION;
  idata.price_ = 82;
  cr_assert( SUCCESS == upd_price( &prm, acc ) );
  cr_assert( sptr->comp_[0].price_ == idata.price_ );
  idata.sym_.k8_[0] = 0UL;
  idata.sym_.k8_[1] = 0UL;
  cr_assert( ERROR_INVALID_ARGUMENT == upd_price( &prm, acc ) );
  idata.sym_.k8_[0] = 1UL;
  cr_assert( ERROR_INVALID_ARGUMENT == upd_price( &prm, acc ) );
  idata.sym_.k8_[1] = 2UL;
  cr_assert( SUCCESS == upd_price( &prm, acc ) );
  prm.data_len = 1;
  cr_assert( ERROR_INVALID_ARGUMENT == upd_price( &prm, acc ) );
  prm.data_len = sizeof( idata );

  // publish a second price as a different publisher
  SolPubkey pkey2 = {.x = { 2, }};
  cr_assert( SUCCESS == pc_price_node_add_publisher(
        sptr, (pc_pub_key_t*)&pkey2 ) );
  idata.price_ = 104;
  idata.conf_  = 11;
  acc[0].key= &pkey2;
  cr_assert( SUCCESS == upd_price( &prm, acc ) );
  cr_assert( sptr->comp_[1].price_ == idata.price_ );
  cr_assert( sptr->comp_[0].price_ == 82L );

}
