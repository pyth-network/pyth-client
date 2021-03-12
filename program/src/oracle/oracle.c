/*
 * BPF pyth price oracle program
 */
#include <solana_sdk.h>
#include "oracle.h"

static bool valid_funding_account( SolAccountInfo *ka )
{
  return ka->is_signer &&
         ka->is_writable;
}

static bool valid_signable_account( SolParameters *prm,
                                    SolAccountInfo *ka,
                                    uint64_t dlen )
{
  return ka->is_signer &&
         ka->is_writable &&
         SolPubkey_same( ka->owner, prm->program_id ) &&
         ka->data_len >= dlen;
}

static bool valid_writable_account( SolParameters *prm,
                                    SolAccountInfo *ka,
                                    uint64_t dlen )
{
  return ka->is_writable &&
         SolPubkey_same( ka->owner, prm->program_id ) &&
         ka->data_len >= dlen;
}

static bool valid_readable_account( SolParameters *prm,
                                    SolAccountInfo *ka,
                                    uint64_t dlen )
{
  return SolPubkey_same( ka->owner, prm->program_id ) &&
         ka->data_len >= dlen;
}

/*
// update aggregate price
static void pc_price_node_upd_agg_price( pc_price_node_t *sptr )
{
  // create sorted permutation array
  uint32_t numa = 0;
  uint32_t aidx[PC_COMP_SIZE];
  for(uint32_t i=0; i != sptr->num_; ++i ) {
    pc_price_t *iptr = &sptr->comp_[i];
    if ( iptr->price_ != PC_INVALID_PRICE ) {
      aidx[numa] = i;
      for(uint32_t j=numa++; j != 0; --j ) {
        if ( sptr->comp_[aidx[j]].price_ <
             sptr->comp_[aidx[j-1]].price_ ) {
          uint32_t t = aidx[j];
          aidx[j] = aidx[j-1];
          aidx[j-1] = t;
        }
      }
    }
  }

  // check for zero contributors
  pc_price_t *aptr = &sptr->agg_;
  if ( numa == 0 ) {
    aptr->price_ = PC_INVALID_PRICE;
    return;
  }

  // pick median value
  uint32_t midx = aidx[numa/2];
  int64_t apx = sptr->comp_[midx].price_;
  if ( numa%2==0 ) {
    apx += sptr->comp_[midx-1].price_;
    apx /= 2;
  }
  aptr->price_ = apx;
}

// remove publisher from price node
static uint64_t pc_price_node_del_publisher( pc_price_node_t *nptr,
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
      // update aggregate price
//      pc_price_node_upd_agg_price( nptr );
      return SUCCESS;
    }
  }
  return PC_RETURN_PUBKEY_NOT_EXISTS;
}
*/

static uint64_t init_mapping( SolParameters *prm, SolAccountInfo *ka )
{
  // Verify that the new account is signed and writable, with correct
  // ownership and size
  if ( prm->ka_num < 2 ||
       !valid_funding_account( &ka[0] ) ||
       !valid_signable_account( prm, &ka[1], sizeof( pc_map_table_t ) ) ) {
    return ERROR_INVALID_ARGUMENT;
  }

  // Check that the account has not already been initialized
  pc_map_table_t *tptr = (pc_map_table_t*)ka[1].data;
  if ( tptr->magic_ != 0 || tptr->ver_ != 0 ) {
    return ERROR_INVALID_ARGUMENT;
  }

  // Initialize by setting to zero again (just in case) and setting
  // the version number
  sol_memset( tptr, 0, sizeof( pc_map_table_t ) );
  tptr->magic_ = PC_MAGIC;
  tptr->ver_   = PC_VERSION;
  return SUCCESS;
}

static uint64_t add_mapping( SolParameters *prm, SolAccountInfo *ka )
{
  // Account (1) is the map list account
  // Account (2) is the tail or last mapping account in the chain
  // Account (3) is the new mapping account and will become the new tail
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
  if ( pptr->magic_ != PC_MAGIC ||
       nptr->magic_ != 0 ||
       pptr->num_ < PC_MAP_NODE_SIZE ||
       nptr->num_   != 0 ||
       !pc_pub_key_is_zero( &pptr->next_ ) ) {
    return ERROR_INVALID_ARGUMENT;
  }
  // Initialize new account and set version number
  sol_memset( nptr, 0, sizeof( pc_map_table_t ) );
  nptr->magic_ = PC_MAGIC;
  nptr->ver_   = PC_VERSION;

  // Set last mapping account to point to this mapping account
  pc_pub_key_t *pkey = (pc_pub_key_t*)ka[1].key;
  pc_pub_key_t *nkey = (pc_pub_key_t*)ka[2].key;
  pc_pub_key_assign( &pptr->next_, nkey );
  return SUCCESS;
}

static uint64_t add_symbol( SolParameters *prm, SolAccountInfo *ka )
{
  // Validate command parameters
  cmd_add_symbol_t *cptr = (cmd_add_symbol_t*)prm->data;
  if ( prm->data_len != sizeof( cmd_add_symbol_t ) ||
       cptr->expo_ > PC_MAX_NUM_DECIMALS ||
       cptr->expo_ < -PC_MAX_NUM_DECIMALS ||
       cptr->ptype_ == PC_PTYPE_UNKNOWN ||
       pc_symbol_is_zero( &cptr->sym_ ) ) {
    return ERROR_INVALID_ARGUMENT;
  }

  // Account (1) is the mapping account that we're going to add to and
  // must be the tail (or last) mapping account in chain
  // Account (2) is the new price account
  // Verify that these are signed, writable accounts with correct ownership
  // and size
  if ( prm->ka_num < 3 ||
       !valid_funding_account( &ka[0] ) ||
       !valid_signable_account( prm, &ka[1], sizeof( pc_map_table_t ) ) ||
       !valid_signable_account( prm, &ka[2], sizeof( pc_price_t ) ) ) {
    return ERROR_INVALID_ARGUMENT;
  }

  // Verify that mapping account is a valid tail account
  // and that the new price account is uninitialized
  pc_map_table_t *mptr = (pc_map_table_t*)ka[1].data;
  pc_price_t *sptr = (pc_price_t*)ka[2].data;
  if ( mptr->magic_ != PC_MAGIC ||
       sptr->magic_ != 0 ) {
    return ERROR_INVALID_ARGUMENT;
  }

  // Initialize symbol account
  sol_memset( sptr, 0, sizeof( pc_price_t ) );
  sptr->magic_ = PC_MAGIC;
  sptr->ver_   = PC_VERSION;
  sptr->expo_  = cptr->expo_;
  sptr->ptype_ = cptr->ptype_;
  pc_symbol_assign( &sptr->sym_, &cptr->sym_ );

  // bind symbol to new account or add new account
  pc_pub_key_t *kptr = (pc_pub_key_t*)ka[2].key;
  uint32_t idx = cptr->sym_.k8_[0] % (uint64_t)PC_MAP_TABLE_SIZE;
  for( uint32_t vidx = mptr->tab_[idx]; vidx; ) {
    pc_map_node_t *nptr = &mptr->nds_[vidx-1];
    vidx = nptr->next_;
    if ( pc_symbol_equal( &cptr->sym_, &nptr->sym_ ) ) {
      pc_pub_key_assign( &sptr->next_, &nptr->price_acc_ );
      pc_pub_key_assign( &nptr->price_acc_, kptr );
      return SUCCESS;
    }
  }

  // add new symbol if not found but only if last
  // mapping account in the chain and if enough space
  if ( mptr->num_ >= PC_MAP_NODE_SIZE ||
       !pc_pub_key_is_zero( &mptr->next_ ) ) {
    return ERROR_INVALID_ARGUMENT;
  }
  uint32_t hidx = ++mptr->num_;
  pc_map_node_t *hn = &mptr->nds_[hidx-1];
  hn->next_ = mptr->tab_[idx];
  pc_symbol_assign( &hn->sym_, &cptr->sym_ );
  pc_pub_key_assign( &hn->price_acc_, kptr );
  mptr->tab_[idx] = hidx;
  return SUCCESS;
}

static uint64_t add_publisher( SolParameters *prm, SolAccountInfo *ka )
{
  // Validate command parameters
  cmd_add_publisher_t *cptr = (cmd_add_publisher_t*)prm->data;
  if ( prm->data_len != sizeof( cmd_add_publisher_t ) ||
       pc_symbol_is_zero( &cptr->sym_ ) ||
       pc_pub_key_is_zero( &cptr->pub_ ) ) {
    return ERROR_INVALID_ARGUMENT;
  }

  // Account (1) is the price account
  // Verify that this is signed, writable with correct ownership
  // and size
  if ( prm->ka_num < 2 ||
       !valid_funding_account( &ka[0] ) ||
       !valid_signable_account( prm, &ka[1], sizeof( pc_price_t ) ) ) {
    return ERROR_INVALID_ARGUMENT;
  }

  // Verify that symbol account is initialized and corresponds to the
  // same symbol and price-type in the instruction parameters
  pc_price_t *sptr = (pc_price_t*)ka[1].data;
  if ( sptr->magic_ != PC_MAGIC ||
       sptr->ver_ != cptr->ver_ ||
       sptr->ptype_ != cptr->ptype_ ||
       !pc_symbol_equal( &sptr->sym_, &cptr->sym_ ) ) {
    return ERROR_INVALID_ARGUMENT;
  }

  // try to add publisher
  for(uint32_t i=0; i != sptr->num_; ++i ) {
    pc_price_comp_t *iptr = &sptr->comp_[i];
    if ( pc_pub_key_equal( &iptr->pub_, &cptr->pub_ ) ) {
      return ERROR_INVALID_ARGUMENT;
    }
  }
  if ( sptr->num_ >= PC_COMP_SIZE ) {
    return ERROR_INVALID_ARGUMENT;
  }
  pc_price_comp_t *iptr = &sptr->comp_[sptr->num_++];
  sol_memset( iptr, 0, sizeof( pc_price_comp_t ) );
  pc_pub_key_assign( &iptr->pub_, &cptr->pub_ );
  return SUCCESS;
}

// update aggregate price
static void upd_aggregate( pc_price_t *ptr,
                           pc_pub_key_t *kptr,
                           uint64_t slot )
{
  // only re-compute aggregate in next slot
  if ( slot <= ptr->curr_slot_ ) {
    return;
  }
  // update aggregate details ready for next slot
  ptr->agg_.pub_slot_ = slot;         // publish slot-time of agg. price
  ptr->valid_slot_ = ptr->curr_slot_; // valid slot-time of agg. price
  ptr->curr_slot_ = slot;             // new accumulating slot-time
  pc_pub_key_assign( &ptr->agg_pub_, kptr );
  for( unsigned i=0; i != ptr->num_; ++i ) {
    pc_price_comp_t *iptr = &ptr->comp_[i];
    iptr->agg_ = iptr->latest_;
  }
  // TODO: compute aggregate price from component prices
  ptr->agg_.price_  = ptr->comp_[0].agg_.price_;
  ptr->agg_.conf_   = ptr->comp_[0].agg_.conf_;
  ptr->agg_.status_ = ptr->comp_[0].agg_.status_;
}

static uint64_t upd_price( SolParameters *prm, SolAccountInfo *ka )
{
  // Validate command parameters
  cmd_upd_price_t *cptr = (cmd_upd_price_t*)prm->data;
  if ( prm->data_len != sizeof( cmd_upd_price_t ) ||
       cptr->status_ == PC_STATUS_UNKNOWN ||
       pc_symbol_is_zero( &cptr->sym_ ) ) {
    return ERROR_INVALID_ARGUMENT;
  }

  // Account (1) is the price account
  // Account (2) is the sysvar_clock account
  // Verify that this is signed, writable with correct ownership
  // and size
  if ( prm->ka_num < 3 ||
       !valid_funding_account( &ka[0] ) ||
       !valid_writable_account( prm, &ka[1], sizeof( pc_price_t ) ) ||
       !pc_pub_key_equal( (pc_pub_key_t*)ka[2].key,
                          (pc_pub_key_t*)sysvar_clock ) ) {
    return ERROR_INVALID_ARGUMENT;
  }

  // Verify that symbol account is initialized and corresponds to the
  // same symbol in the instruction parameters
  pc_price_t *pptr = (pc_price_t*)ka[1].data;
  if ( pptr->magic_ != PC_MAGIC ||
       pptr->ver_ > PC_VERSION ||
       pptr->ptype_ != cptr->ptype_ ||
       !pc_symbol_equal( &pptr->sym_, &cptr->sym_ ) ) {
    return ERROR_INVALID_ARGUMENT;
  }

  // verify that publisher is valid
  uint32_t i = 0;
  pc_pub_key_t *kptr = (pc_pub_key_t*)ka[0].key;
  for( i=0; i != pptr->num_; ++i ) {
    pc_price_comp_t *iptr = &pptr->comp_[i];
    if ( pc_pub_key_equal( kptr, &iptr->pub_ ) ) {
      break;
    }
  }
  if ( i == pptr->num_ ) {
    return ERROR_INVALID_ARGUMENT;
  }

  // update aggregate price as necessary
  pc_price_info_t *fptr = &pptr->comp_[i].latest_;
  sysvar_clock_t *sptr = (sysvar_clock_t*)ka[2].data;
  if ( sptr->slot_ > pptr->curr_slot_ ) {
    upd_aggregate( pptr, kptr, sptr->slot_ );
  }

  // find and update component price
  fptr->price_    = cptr->price_;
  fptr->conf_     = cptr->conf_;
  fptr->status_   = cptr->status_;
  fptr->pub_slot_ = sptr->slot_;
  return SUCCESS;
}

static uint64_t dispatch_1( SolParameters *prm, SolAccountInfo *ka )
{
  uint64_t res = ERROR_INVALID_ARGUMENT;
  cmd_hdr_t *hdr = (cmd_hdr_t*)prm->data;
  switch(hdr->cmd_) {
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

static uint64_t dispatch( SolParameters *prm, SolAccountInfo *ka )
{
  if (prm->data_len < sizeof(cmd_hdr_t) ) {
    return ERROR_INVALID_ARGUMENT;
  }
  cmd_hdr_t *hdr = (cmd_hdr_t*)prm->data;
  uint64_t res = ERROR_INVALID_ARGUMENT;
  switch( hdr->ver_ ) {
    case PC_VERSION_1: res = dispatch_1( prm, ka ); break;
    default: break;
  }
  return res;
}

extern uint64_t entrypoint(const uint8_t *input)
{
  SolAccountInfo ka[3];
  SolParameters prm[1];
  prm->ka = ka;
  if (!sol_deserialize(input, prm, SOL_ARRAY_SIZE(ka))) {
    return ERROR_INVALID_ARGUMENT;
  }
  return dispatch( prm, ka );
}
