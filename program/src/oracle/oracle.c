/*
 * BPF pyth price oracle program
 */
#include <solana_sdk.h>
#include "oracle.h"
#include "upd_aggregate.h"

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

static uint64_t init_mapping( SolParameters *prm, SolAccountInfo *ka )
{
  // Verify that the new account is signed and writable, with correct
  // ownership and size
  if ( prm->ka_num != 2 ||
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
  cmd_hdr_t *hdr = (cmd_hdr_t*)prm->data;
  sol_memset( tptr, 0, sizeof( pc_map_table_t ) );
  tptr->magic_ = PC_MAGIC;
  tptr->ver_   = hdr->ver_;
  tptr->type_  = PC_ACCTYPE_MAPPING;
  tptr->size_  = sizeof( pc_map_table_t ) - sizeof( tptr->prod_ );
  return SUCCESS;
}

static uint64_t add_mapping( SolParameters *prm, SolAccountInfo *ka )
{
  // Account (1) is the tail or last mapping account in the chain
  // Account (2) is the new mapping account and will become the new tail
  // Verify that these are signed, writable accounts with correct ownership
  // and size
  if ( prm->ka_num != 3 ||
       !valid_funding_account( &ka[0] ) ||
       !valid_signable_account( prm, &ka[1], sizeof( pc_map_table_t ) ) ||
       !valid_signable_account( prm, &ka[2], sizeof( pc_map_table_t ) ) ) {
    return ERROR_INVALID_ARGUMENT;
  }
  // Verify that last mapping account in chain is initialized, full
  // and not pointing to a another account in the chain
  // Also verify that the new account is uninitialized
  cmd_hdr_t *hdr = (cmd_hdr_t*)prm->data;
  pc_map_table_t *pptr = (pc_map_table_t*)ka[1].data;
  pc_map_table_t *nptr = (pc_map_table_t*)ka[2].data;
  if ( pptr->magic_ != PC_MAGIC ||
       pptr->ver_   != hdr->ver_ ||
       pptr->type_  != PC_ACCTYPE_MAPPING ||
       nptr->magic_ != 0 ||
       pptr->num_ < PC_MAP_TABLE_SIZE ||
       nptr->num_   != 0 ||
       !pc_pub_key_is_zero( &pptr->next_ ) ) {
    return ERROR_INVALID_ARGUMENT;
  }
  // Initialize new account and set version number
  sol_memset( nptr, 0, sizeof( pc_map_table_t ) );
  nptr->magic_ = PC_MAGIC;
  nptr->ver_   = hdr->ver_;
  nptr->type_  = PC_ACCTYPE_MAPPING;
  nptr->size_  = sizeof( pc_map_table_t ) - sizeof( nptr->prod_ );

  // Set last mapping account to point to this mapping account
  pc_pub_key_t *pkey = (pc_pub_key_t*)ka[1].key;
  pc_pub_key_t *nkey = (pc_pub_key_t*)ka[2].key;
  pc_pub_key_assign( &pptr->next_, nkey );
  return SUCCESS;
}

static uint64_t add_product( SolParameters *prm, SolAccountInfo *ka )
{
  // Account (1) is the mapping account that we're going to add to and
  // must be the tail (or last) mapping account in chain
  // Account (2) is the new product account
  // Verify that these are signed, writable accounts with correct ownership
  // and size
  if ( prm->ka_num != 3 ||
       !valid_funding_account( &ka[0] ) ||
       !valid_signable_account( prm, &ka[1], sizeof( pc_map_table_t ) ) ||
       !valid_signable_account( prm, &ka[2], PC_PROD_ACC_SIZE ) ) {
    return ERROR_INVALID_ARGUMENT;
  }

  // Verify that mapping account is a valid tail account
  // that the new product account is uninitialized and that there is space
  // in the mapping account
  cmd_hdr_t *hdr = (cmd_hdr_t*)prm->data;
  pc_map_table_t *mptr = (pc_map_table_t*)ka[1].data;
  pc_prod_t *pptr = (pc_prod_t*)ka[2].data;
  if ( mptr->magic_ != PC_MAGIC ||
       mptr->ver_ != hdr->ver_ ||
       mptr->type_ != PC_ACCTYPE_MAPPING ||
       pptr->magic_ != 0 ||
       mptr->num_ >= PC_MAP_TABLE_SIZE ) {
    return ERROR_INVALID_ARGUMENT;
  }

  // Initialize product account
  sol_memset( pptr, 0, PC_PROD_ACC_SIZE );
  pptr->magic_ = PC_MAGIC;
  pptr->ver_   = hdr->ver_;
  pptr->type_  = PC_ACCTYPE_PRODUCT;
  pptr->size_  = sizeof( pc_prod_t );

  // finally add mapping account link
  pc_pub_key_assign( &mptr->prod_[mptr->num_++], (pc_pub_key_t*)ka[2].key );
  mptr->size_  = sizeof( pc_map_table_t ) - sizeof( mptr->prod_ ) +
    mptr->num_ * sizeof( pc_pub_key_t );
  return SUCCESS;
}

#define PC_ADD_STR \
  tag = (pc_str_t*)src;\
  tag_len = 1UL + (uint64_t)tag->len_;\
  if ( &src[tag_len] > end ) return ERROR_INVALID_ARGUMENT;\
  sol_memcpy( tgt, tag, tag_len );\
  tgt += tag_len;\
  src += tag_len;\

static uint64_t upd_product( SolParameters *prm, SolAccountInfo *ka )
{
  // Account (1) is the existing product account
  // Verify that these are signed, writable accounts with correct ownership
  // and size
  if ( prm->ka_num != 2 ||
       !valid_funding_account( &ka[0] ) ||
       !valid_signable_account( prm, &ka[1], PC_PROD_ACC_SIZE ) ) {
    return ERROR_INVALID_ARGUMENT;
  }

  // verify that product account is valid
  cmd_hdr_t *hdr = (cmd_hdr_t*)prm->data;
  pc_prod_t *pptr = (pc_prod_t*)ka[1].data;
  if ( pptr->magic_ != PC_MAGIC ||
       pptr->ver_ != hdr->ver_ ||
       pptr->type_ != PC_ACCTYPE_PRODUCT ) {
    return ERROR_INVALID_ARGUMENT;
  }

  // unpack and verify attribute set and ssign to product account
  if ( prm->data_len < sizeof( cmd_upd_product_t ) ||
       prm->data_len > PC_PROD_ACC_SIZE +
         sizeof( cmd_upd_product_t ) - sizeof( pc_prod_t ) ) {
    return ERROR_INVALID_ARGUMENT;
  }
  cmd_upd_product_t *cptr = (cmd_upd_product_t*)prm->data;
  pptr->size_ = sizeof ( pc_prod_t ) + prm->data_len -
    sizeof( cmd_upd_product_t );
  uint8_t *tgt = (uint8_t*)pptr + sizeof( pc_prod_t );
  const uint8_t *src = prm->data + sizeof( cmd_upd_product_t );
  const uint8_t *end = prm->data + prm->data_len;
  const pc_str_t *tag;
  uint64_t tag_len;
  while( src != end ) {
    // check key string
    PC_ADD_STR
    // check value string
    PC_ADD_STR
  }
  return SUCCESS;
}

static uint64_t add_price( SolParameters *prm, SolAccountInfo *ka )
{
  // Validate command parameters
  cmd_add_price_t *cptr = (cmd_add_price_t*)prm->data;
  if ( prm->data_len != sizeof( cmd_add_price_t ) ||
       cptr->expo_ > PC_MAX_NUM_DECIMALS ||
       cptr->expo_ < -PC_MAX_NUM_DECIMALS ||
       cptr->ptype_ == PC_PTYPE_UNKNOWN ) {
    return ERROR_INVALID_ARGUMENT;
  }

  // Account (1) is the product account that we're going to add to
  // Account (2) is the new price account
  // Verify that these are signed, writable accounts with correct ownership
  // and size
  if ( prm->ka_num != 3 ||
       !valid_funding_account( &ka[0] ) ||
       !valid_signable_account( prm, &ka[1], PC_PROD_ACC_SIZE ) ||
       !valid_signable_account( prm, &ka[2], sizeof( pc_price_t ) ) ) {
    return ERROR_INVALID_ARGUMENT;
  }

  // Verify that the product account is valid
  // and that the new price account is uninitialized
  pc_prod_t  *pptr = (pc_prod_t*)ka[1].data;
  pc_price_t *sptr = (pc_price_t*)ka[2].data;
  if ( pptr->magic_ != PC_MAGIC ||
       pptr->ver_ != cptr->ver_ ||
       pptr->type_ != PC_ACCTYPE_PRODUCT ||
       sptr->magic_ != 0 ) {
    return ERROR_INVALID_ARGUMENT;
  }

  // Initialize symbol account
  sol_memset( sptr, 0, sizeof( pc_price_t ) );
  sptr->magic_ = PC_MAGIC;
  sptr->ver_   = cptr->ver_;
  sptr->type_  = PC_ACCTYPE_PRICE;
  sptr->size_  = sizeof( pc_price_t ) - sizeof( sptr->comp_ );
  sptr->expo_  = cptr->expo_;
  sptr->ptype_ = cptr->ptype_;
  pc_pub_key_assign( &sptr->prod_, (pc_pub_key_t*)ka[1].key );

  // bind price account to product account
  pc_pub_key_assign( &sptr->next_, &pptr->px_acc_ );
  pc_pub_key_assign( &pptr->px_acc_, (pc_pub_key_t*)ka[2].key );
  return SUCCESS;
}

static uint64_t init_price( SolParameters *prm, SolAccountInfo *ka )
{
  // Validate command parameters
  cmd_init_price_t *cptr = (cmd_init_price_t*)prm->data;
  if ( prm->data_len != sizeof( cmd_init_price_t ) ||
       cptr->expo_ > PC_MAX_NUM_DECIMALS ||
       cptr->expo_ < -PC_MAX_NUM_DECIMALS ) {
    return ERROR_INVALID_ARGUMENT;
  }

  // Account (1) is the price account to (re)initialize
  // Verify that these are signed, writable accounts with correct ownership
  // and size
  if ( prm->ka_num != 2 ||
       !valid_funding_account( &ka[0] ) ||
       !valid_signable_account( prm, &ka[1], sizeof( pc_price_t ) )) {
    return ERROR_INVALID_ARGUMENT;
  }

  // Verify that the price account is initialized
  pc_price_t *sptr = (pc_price_t*)ka[1].data;
  if ( sptr->magic_ != PC_MAGIC ||
       sptr->ver_ != cptr->ver_ ||
       sptr->type_ != PC_ACCTYPE_PRICE ||
       sptr->ptype_ != cptr->ptype_ ) {
    return ERROR_INVALID_ARGUMENT;
  }

  // (re)initialize price exponent and clear-down all quotes
  sptr->expo_  = cptr->expo_;
  sptr->last_slot_  = 0UL;
  sptr->valid_slot_ = 0UL;
  sptr->agg_.pub_slot_ = 0UL;
  sptr->prev_slot_  = 0UL;
  sptr->prev_price_ = 0L;
  sptr->prev_conf_  = 0L;
  sol_memset( &sptr->twap_, 0, sizeof( pc_ema_t ) );
  sol_memset( &sptr->twac_, 0, sizeof( pc_ema_t ) );
  sol_memset( &sptr->agg_, 0, sizeof( pc_price_info_t ) );
  for(unsigned i=0; i != sptr->num_; ++i ) {
    pc_price_comp_t *iptr = &sptr->comp_[i];
    sol_memset( &iptr->agg_, 0, sizeof( pc_price_info_t ) );
    sol_memset( &iptr->latest_, 0, sizeof( pc_price_info_t ) );
  }
  return SUCCESS;
}

static uint64_t add_publisher( SolParameters *prm, SolAccountInfo *ka )
{
  // Validate command parameters
  cmd_add_publisher_t *cptr = (cmd_add_publisher_t*)prm->data;
  if ( prm->data_len != sizeof( cmd_add_publisher_t ) ||
       pc_pub_key_is_zero( &cptr->pub_ ) ) {
    return ERROR_INVALID_ARGUMENT;
  }

  // Account (1) is the price account
  // Verify that this is signed, writable with correct ownership
  // and size
  if ( prm->ka_num != 2 ||
       !valid_funding_account( &ka[0] ) ||
       !valid_signable_account( prm, &ka[1], sizeof( pc_price_t ) ) ) {
    return ERROR_INVALID_ARGUMENT;
  }

  // Verify that symbol account is initialized and corresponds to the
  // same symbol and price-type in the instruction parameters
  pc_price_t *sptr = (pc_price_t*)ka[1].data;
  if ( sptr->magic_ != PC_MAGIC ||
       sptr->ver_ != cptr->ver_ ||
       sptr->type_ != PC_ACCTYPE_PRICE ) {
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

  // update size of account
  sptr->size_ = sizeof( pc_price_t ) - sizeof( sptr->comp_ ) +
    sptr->num_ * sizeof( pc_price_comp_t );
  return SUCCESS;
}

// remove publisher from price node
static uint64_t del_publisher( SolParameters *prm, SolAccountInfo *ka )
{
  // Validate command parameters
  cmd_del_publisher_t *cptr = (cmd_del_publisher_t*)prm->data;
  if ( prm->data_len != sizeof( cmd_del_publisher_t ) ||
       pc_pub_key_is_zero( &cptr->pub_ ) ) {
    return ERROR_INVALID_ARGUMENT;
  }

  // Account (1) is the price account
  // Verify that this is signed, writable with correct ownership
  // and size
  if ( prm->ka_num != 2 ||
       !valid_funding_account( &ka[0] ) ||
       !valid_signable_account( prm, &ka[1], sizeof( pc_price_t ) ) ) {
    return ERROR_INVALID_ARGUMENT;
  }

  // Verify that symbol account is initialized and corresponds to the
  // same symbol and price-type in the instruction parameters
  pc_price_t *sptr = (pc_price_t*)ka[1].data;
  if ( sptr->magic_ != PC_MAGIC ||
       sptr->ver_ != cptr->ver_ ||
       sptr->type_ != PC_ACCTYPE_PRICE ) {
    return ERROR_INVALID_ARGUMENT;
  }

  // try to remove publisher
  for(uint32_t i=0; i != sptr->num_; ++i ) {
    pc_price_comp_t *iptr = &sptr->comp_[i];
    if ( pc_pub_key_equal( &iptr->pub_, &cptr->pub_ ) ) {
      for( unsigned j=i+1; j != sptr->num_; ++j ) {
        pc_price_comp_t *jptr = &sptr->comp_[j];
        iptr[0] = jptr[0];
        iptr = jptr;
      }
      --sptr->num_;
      sol_memset( iptr, 0, sizeof( pc_price_comp_t ) );
      // update size of account
      sptr->size_ = sizeof( pc_price_t ) - sizeof( sptr->comp_ ) +
        sptr->num_ * sizeof( pc_price_comp_t );
      return SUCCESS;
    }
  }
  return ERROR_INVALID_ARGUMENT;
}

static uint64_t init_test( SolParameters *prm, SolAccountInfo *ka )
{
  // Account (1) is the test account
  // Verify that this is signed, writable with correct ownership
  // and size
  cmd_hdr_t *hdr = (cmd_hdr_t*)prm->data;
  if ( prm->ka_num < 2 ||
       !valid_funding_account( &ka[0] ) ||
       !valid_signable_account( prm, &ka[1], sizeof( pc_price_t ) ) ) {
    return ERROR_INVALID_ARGUMENT;
  }

  // Check that the account has not already been initialized
  pc_price_t *tptr = (pc_price_t*)ka[1].data;
  if ( tptr->magic_ != 0 || tptr->ver_ != 0 ) {
    return ERROR_INVALID_ARGUMENT;
  }

  // initialize account header and price factor table
  sol_memset( tptr, 0, sizeof( pc_price_t ) );
  tptr->magic_ = PC_MAGIC;
  tptr->ver_   = hdr->ver_;
  tptr->type_  = PC_ACCTYPE_TEST;
  return SUCCESS;
}

static uint64_t upd_test( SolParameters *prm, SolAccountInfo *ka )
{
  // Account (1) is the test account
  // Verify that this is signed, writable with correct ownership
  // and size
  if ( prm->ka_num !=2 ||
       !valid_funding_account( &ka[0] ) ||
       !valid_signable_account( prm, &ka[1], sizeof( pc_price_t ) ) ) {
    return ERROR_INVALID_ARGUMENT;
  }

  // validate command params and account headers
  cmd_upd_test_t *cmd = (cmd_upd_test_t*)prm->data;
  pc_price_t *px = (pc_price_t*)ka[1].data;
  if ( prm->data_len != sizeof( cmd_upd_test_t ) ||
       px->magic_ != PC_MAGIC ||
       px->ver_ != cmd->ver_ ||
       px->type_ != PC_ACCTYPE_TEST ) {
    return ERROR_INVALID_ARGUMENT;
  }

  // compute aggregate price
  uint64_t slot = 1000UL;
  px->last_slot_= slot;
  px->agg_.pub_slot_ = slot;
  px->expo_      = cmd->expo_;
  px->num_       = cmd->num_;
  for( uint32_t i=0; i != cmd->num_; ++i ) {
    pc_price_comp_t *ptr = &px->comp_[i];
    ptr->latest_.status_ = PC_STATUS_TRADING;
    ptr->latest_.price_  = cmd->price_[i];
    ptr->latest_.conf_   = cmd->conf_[i];
    ptr->latest_.pub_slot_ = slot + (uint64_t)cmd->slot_diff_[i];
  }
  upd_aggregate( px, slot+1 );

  return SUCCESS;
}

static uint64_t upd_price( SolParameters *prm, SolAccountInfo *ka )
{
  // Validate command parameters
  cmd_upd_price_t *cptr = (cmd_upd_price_t*)prm->data;
  if ( prm->data_len != sizeof( cmd_upd_price_t ) ) {
    return ERROR_INVALID_ARGUMENT;
  }

  // Account (1) is the price account
  // Account (2) is the sysvar_clock account
  // Verify that this is signed, writable with correct ownership and size
  uint32_t clock_idx = prm->ka_num == 3 ? 2 : 3;
  if ( (prm->ka_num != 3 && prm->ka_num != 4) ||
       !valid_funding_account( &ka[0] ) ||
       !valid_writable_account( prm, &ka[1], sizeof( pc_price_t ) ) ||
       !pc_pub_key_equal( (pc_pub_key_t*)ka[clock_idx].key,
                          (pc_pub_key_t*)sysvar_clock ) ) {
    return ERROR_INVALID_ARGUMENT;
  }

  // Verify that symbol account is initialized and corresponds to the
  // same symbol in the instruction parameters
  pc_price_t *pptr = (pc_price_t*)ka[1].data;
  if ( pptr->magic_ != PC_MAGIC ||
       pptr->ver_ != cptr->ver_ ||
       pptr->type_ != PC_ACCTYPE_PRICE ) {
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

  // reject if this price corresponds to the same or earlier time
  pc_price_info_t *fptr = &pptr->comp_[i].latest_;
  sysvar_clock_t *sptr = (sysvar_clock_t*)ka[clock_idx].data;
  if ( cptr->cmd_ == e_cmd_upd_price &&
       cptr->pub_slot_ <= fptr->pub_slot_ ) {
    return ERROR_INVALID_ARGUMENT;
  }

  // update aggregate price as necessary
  if ( sptr->slot_ > pptr->agg_.pub_slot_ ) {
    upd_aggregate( pptr, sptr->slot_ );
  }

  // update component price if required
  if ( cptr->cmd_ == e_cmd_upd_price ) {
    fptr->price_    = cptr->price_;
    fptr->conf_     = cptr->conf_;
    fptr->status_   = cptr->status_;
    fptr->pub_slot_ = cptr->pub_slot_;
  }
  return SUCCESS;
}

static uint64_t dispatch( SolParameters *prm, SolAccountInfo *ka )
{
  if (prm->data_len < sizeof(cmd_hdr_t) ) {
    return ERROR_INVALID_ARGUMENT;
  }
  cmd_hdr_t *hdr = (cmd_hdr_t*)prm->data;
  if ( hdr->ver_ != PC_VERSION ) {
    return ERROR_INVALID_ARGUMENT;
  }
  switch(hdr->cmd_) {
    case e_cmd_upd_price:
    case e_cmd_agg_price:     return upd_price( prm, ka );
    case e_cmd_init_mapping:  return init_mapping( prm, ka );
    case e_cmd_add_mapping:   return add_mapping( prm, ka );
    case e_cmd_add_product:   return add_product( prm, ka );
    case e_cmd_upd_product:   return upd_product( prm, ka );
    case e_cmd_add_price:     return add_price( prm, ka );
    case e_cmd_add_publisher: return add_publisher( prm, ka );
    case e_cmd_del_publisher: return del_publisher( prm, ka );
    case e_cmd_init_price:    return init_price( prm, ka );
    case e_cmd_init_test:     return init_test( prm, ka );
    case e_cmd_upd_test:      return upd_test( prm, ka );
    default:                  return ERROR_INVALID_ARGUMENT;
  }
}

extern uint64_t entrypoint(const uint8_t *input)
{
  SolAccountInfo ka[4];
  SolParameters prm[1];
  prm->ka = ka;
  if (!sol_deserialize(input, prm, SOL_ARRAY_SIZE(ka))) {
    return ERROR_INVALID_ARGUMENT;
  }
  return dispatch( prm, ka );
}

