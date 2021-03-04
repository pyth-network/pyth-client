/*
 * BPF pyth price oracle program
 */
#include <solana_sdk.h>
#include "oracle.h"

// get symbol account from symbol
pc_pub_key_t *pc_map_table_get_account( pc_map_table_t *tptr,
                                        pc_symbol_t *sptr )
{
  uint32_t idx = sptr->k8_[0] % (uint64_t)PC_MAP_TABLE_SIZE;
  for( uint32_t vidx = tptr->tab_[idx]; vidx; ) {
    pc_map_node_t *nptr = &tptr->nds_[vidx-1];
    vidx = nptr->next_;
    if ( pc_symbol_equal( sptr, &nptr->sym_ ) ) {
      return &nptr->acc_;
    }
  }
  return (pc_pub_key_t*)0;
}

// add new symbol/account in mapping table
uint64_t pc_map_table_add_symbol( pc_map_table_t *tptr,
                                  pc_symbol_t *sptr,
                                  pc_pub_key_t *aptr )
{
  uint32_t idx = sptr->k8_[0] % (uint64_t)PC_MAP_TABLE_SIZE;
  for( uint32_t vidx = tptr->tab_[idx]; vidx; ) {
    pc_map_node_t *nptr = &tptr->nds_[vidx-1];
    vidx = nptr->next_;
    if ( pc_symbol_equal( sptr, &nptr->sym_ ) ) {
      return PC_RETURN_SYMBOL_EXISTS;
    }
  }
  if ( tptr->num_ >= PC_MAP_NODE_SIZE ) {
    return PC_RETURN_MAX_CAPACITY;
  }
  uint32_t hidx = ++tptr->num_;
  pc_map_node_t *hn = &tptr->nds_[hidx-1];
  hn->next_ = tptr->tab_[idx];
  pc_symbol_assign( &hn->sym_, sptr );
  pc_pub_key_assign( &hn->acc_, aptr );
  tptr->tab_[idx] = hidx;
  return PC_RETURN_SUCCESS;
}

// add publisher to price node
uint64_t pc_price_node_add_publisher( pc_price_node_t *nptr,
                                      pc_pub_key_t *pub )
{
  for(uint32_t i=0; i != nptr->num_; ++i ) {
    pc_price_t *iptr = &nptr->comp_[i];
    if ( pc_pub_key_equal( &iptr->pub_, pub ) ) {
      return PC_RETURN_PUBKEY_EXISTS;
    }
  }
  if ( nptr->num_ >= PC_COMP_SIZE ) {
    return PC_RETURN_MAX_CAPACITY;
  }
  pc_price_t *iptr = &nptr->comp_[nptr->num_++];
  pc_pub_key_assign( &iptr->pub_, pub );
  iptr->price_ = PC_INVALID_PRICE;
  iptr->conf_  = 0UL;
  return PC_RETURN_SUCCESS;
}

// remove publisher from price node
uint64_t pc_price_node_del_publisher( pc_price_node_t *nptr,
                                      pc_pub_key_t *pub )
{
  for(uint32_t i=0; i != PC_COMP_SIZE; ++i ) {
    pc_price_t *iptr = &nptr->comp_[i];
    if ( pc_pub_key_equal( pub, &iptr->pub_ ) ) {
      for( unsigned j=i+1; j != nptr->num_; ++j ) {
        pc_price_t *jptr = &nptr->comp_[j];
        pc_pub_key_assign( &iptr->pub_, &jptr->pub_ );
        iptr->price_ = jptr->price_;
        iptr->conf_  = jptr->conf_;
        iptr = jptr;
      }
      --nptr->num_;
      iptr = &nptr->comp_[nptr->num_];
      pc_pub_key_set_zero( &iptr->pub_ );
      iptr->price_ = 0L;
      iptr->conf_  = 0UL;
      return PC_RETURN_SUCCESS;
    }
  }
  return PC_RETURN_PUBKEY_NOT_EXISTS;
}


bool valid_funding_account( SolAccountInfo *ka )
{
  return ka->is_signer &&
         ka->is_writable;
}

bool valid_signable_account( SolParameters *prm,
                             SolAccountInfo *ka,
                             uint64_t dlen )
{
  return ka->is_signer &&
         ka->is_writable &&
         SolPubkey_same( ka->owner, prm->program_id ) &&
         ka->data_len >= dlen;
}

bool valid_writable_account( SolParameters *prm,
                             SolAccountInfo *ka,
                             uint64_t dlen )
{
  return ka->is_writable &&
         SolPubkey_same( ka->owner, prm->program_id ) &&
         ka->data_len >= dlen;
}

uint64_t init_mapping( SolParameters *prm, SolAccountInfo *ka )
{
  // Verify that the new account is signed and writable, with correct
  // ownership and size
  if ( prm->ka_num < 2 ||
       !valid_funding_account( &ka[0] ) ||
       !valid_signable_account( prm, &ka[1], sizeof( pc_map_table_t) ) ) {
    return ERROR_INVALID_ARGUMENT;
  }

  // Check that the account has not already been initialized
  pc_map_table_t *tptr = (pc_map_table_t*)ka[1].data;
  if ( tptr->ver_ != 0 ) {
    return ERROR_INVALID_ARGUMENT;
  }

  // Initialize by setting to zero again (just in case) and setting
  // the version number
  sol_memset( tptr, 0, sizeof( pc_map_table_t ) );
  tptr->ver_ = PC_VERSION;
  return PC_RETURN_SUCCESS;
}

uint64_t add_mapping( SolParameters *prm, SolAccountInfo *ka )
{
  // Account (1) is the tail or last mapping account in the chain
  // Account (2) is the new mapping account and will become the new tail
  // Verify that these are signed, writable accounts with correct ownership
  // and size
  if ( prm->ka_num < 3 ||
       !valid_funding_account( &ka[0] ) ||
       !valid_signable_account( prm, &ka[1], sizeof( pc_map_table_t ) ) ||
       !valid_signable_account( prm, &ka[2], sizeof( pc_map_table_t ) ) ) {
    return ERROR_INVALID_ARGUMENT;
  }
  // Verify that last mapping account in chain is initialized, full
  // and not pointing to a another account in the chain
  // Also verify that the new account is uninitialized
  pc_map_table_t *pptr = (pc_map_table_t*)ka[1].data;
  pc_map_table_t *nptr = (pc_map_table_t*)ka[2].data;
  if ( pptr->ver_ == 0 ||
       pptr->ver_ > PC_VERSION ||
       pptr->num_ < PC_MAP_NODE_SIZE ||
       !pc_pub_key_is_zero( &pptr->next_ ) ||
       nptr->ver_ != 0 ||
       nptr->num_ != 0 ) {
    return ERROR_INVALID_ARGUMENT;
  }
  // Initialize new account and set version number
  sol_memset( nptr, 0, sizeof( pc_map_table_t ) );
  nptr->ver_ = PC_VERSION;

  // Set last mapping account to point to this mapping account
  pc_pub_key_t *pkey = (pc_pub_key_t*)ka[1].key;
  pc_pub_key_t *nkey = (pc_pub_key_t*)ka[2].key;
  pc_pub_key_assign( &pptr->next_, nkey );
  pc_pub_key_assign( &nptr->prev_, pkey );
  return PC_RETURN_SUCCESS;
}

uint64_t add_symbol( SolParameters *prm, SolAccountInfo *ka )
{
  // Validate command parameters
  cmd_add_symbol_t *cptr = (cmd_add_symbol_t*)prm->data;
  if ( prm->data_len != sizeof( cmd_add_symbol_t ) ||
       cptr->expo_ > PC_MAX_NUM_DECIMALS ||
       cptr->expo_ < -PC_MAX_NUM_DECIMALS ||
       pc_symbol_is_zero( &cptr->sym_ ) ) {
    return ERROR_INVALID_ARGUMENT;
  }

  // Account (1) is the mapping account that we're going to add to and
  // must be the tail (or last) mapping account in chain
  // Account (2) is the new symbol account
  // Verify that these are signed, writable accounts with correct ownership
  // and size
  if ( prm->ka_num < 3 ||
       !valid_funding_account( &ka[0] ) ||
       !valid_signable_account( prm, &ka[1], sizeof( pc_map_table_t ) ) ||
       !valid_signable_account( prm, &ka[2], sizeof( pc_price_node_t ) ) ) {
    return ERROR_INVALID_ARGUMENT;
  }

  // Verify that mapping account is a valid tail account
  pc_map_table_t *mptr = (pc_map_table_t*)ka[1].data;
  if ( mptr->ver_ == 0 ||
       mptr->ver_ > PC_VERSION ||
       !pc_pub_key_is_zero( &mptr->next_ ) ) {
    return ERROR_INVALID_ARGUMENT;
  }

  // Verify that symbol account is uninitialized
  pc_price_node_t *sptr = (pc_price_node_t*)ka[2].data;
  if ( sptr->ver_ != 0 ||
       sptr->num_ != 0 ) {
    return ERROR_INVALID_ARGUMENT;
  }

  // Add symbol to mapping table
  pc_pub_key_t *aptr = (pc_pub_key_t*)ka[2].key;
  uint64_t res = pc_map_table_add_symbol( mptr, &cptr->sym_, aptr );
  if ( res != PC_RETURN_SUCCESS ) {
    return res;
  }

  // Initialize symbol account
  sol_memset( sptr, 0, sizeof( pc_price_node_t ) );
  sptr->ver_  = PC_VERSION;
  sptr->expo_ = cptr->expo_;
  pc_symbol_assign( &sptr->sym_, &cptr->sym_ );

  return PC_RETURN_SUCCESS;
}

uint64_t add_publisher( SolParameters *prm, SolAccountInfo *ka )
{
  // Validate command parameters
  cmd_add_publisher_t *cptr = (cmd_add_publisher_t*)prm->data;
  if ( prm->data_len != sizeof( cmd_add_publisher_t ) ||
       pc_symbol_is_zero( &cptr->sym_ ) ||
       pc_pub_key_is_zero( &cptr->pub_ ) ) {
    return ERROR_INVALID_ARGUMENT;
  }

  // Account (1) is the symbol account
  // Verify that this is signed, writable with correct ownership
  // and size
  if ( prm->ka_num < 2 ||
       !valid_funding_account( &ka[0] ) ||
       !valid_signable_account( prm, &ka[1], sizeof( pc_price_node_t ) ) ) {
    return ERROR_INVALID_ARGUMENT;
  }

  // Verify that symbol account is initialized and corresponds to the
  // same symbol in the instruction parameters
  pc_price_node_t *sptr = (pc_price_node_t*)ka[1].data;
  if ( sptr->ver_ == 0 ||
       sptr->ver_ > PC_VERSION ||
       !pc_symbol_equal( &sptr->sym_, &cptr->sym_ ) ) {
    return ERROR_INVALID_ARGUMENT;
  }

  // try to add publisher
  uint64_t res = pc_price_node_add_publisher( sptr, &cptr->pub_ );
  return res;
}

uint64_t upd_price( SolParameters *prm, SolAccountInfo *ka )
{
  // Validate command parameters
  cmd_upd_price_t *cptr = (cmd_upd_price_t*)prm->data;
  if ( prm->data_len != sizeof( cmd_upd_price_t ) ||
       pc_symbol_is_zero( &cptr->sym_ ) ) {
    return ERROR_INVALID_ARGUMENT;
  }

  // Account (1) is the symbol account
  // Verify that this is signed, writable with correct ownership
  // and size
  if ( prm->ka_num < 2 ||
       !valid_funding_account( &ka[0] ) ||
       !valid_writable_account( prm, &ka[1], sizeof( pc_price_node_t ) ) ) {
    return ERROR_INVALID_ARGUMENT;
  }

  // Verify that symbol account is initialized and corresponds to the
  // same symbol in the instruction parameters
  pc_price_node_t *sptr = (pc_price_node_t*)ka[1].data;
  if ( sptr->ver_ == 0 ||
       sptr->ver_ > PC_VERSION ||
       !pc_symbol_equal( &sptr->sym_, &cptr->sym_ ) ) {
    return ERROR_INVALID_ARGUMENT;
  }

  // try to set price with funding account as publisher key
  pc_pub_key_t *pub = (pc_pub_key_t*)ka[0].key;
  for(uint32_t i=0; i != sptr->num_; ++i ) {
    pc_price_t *iptr = &sptr->comp_[i];
    if ( pc_pub_key_equal( pub, &iptr->pub_ ) ) {
      iptr->price_ = cptr->price_;
      iptr->conf_  = cptr->conf_;
      return PC_RETURN_SUCCESS;
    }
  }
  return ERROR_INVALID_ARGUMENT;
}

extern uint64_t entrypoint(const uint8_t *input)
{
  SolAccountInfo ka[3];
  SolParameters prm[1];
  prm->ka = ka;

  if (!sol_deserialize(input, prm, SOL_ARRAY_SIZE(ka))) {
    return ERROR_INVALID_ARGUMENT;
  }
  if (prm->data_len < sizeof(int32_t) ) {
    return ERROR_INVALID_ARGUMENT;
  }
  uint32_t cmd = *(uint32_t*)prm->data;
  uint64_t res = ERROR_INVALID_ARGUMENT;
  switch(cmd) {
    case e_cmd_upd_price:{
      res = upd_price( prm, ka );
      break;
    }
    case e_cmd_init_mapping:{
      res = init_mapping( prm, ka );
      break;
    }
    case e_cmd_add_mapping:{
      res = add_mapping( prm, ka );
      break;
    }
    case e_cmd_add_symbol:{
      res = add_symbol( prm, ka );
      break;
    }
    case e_cmd_add_publisher:{
      res = add_publisher( prm, ka );
      break;
    }
    default: break;
  }
  return res;
}
