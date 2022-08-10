/*
 * BPF pyth price oracle program
 */
#include <solana_sdk.h>
#include "oracle.h"
#include "upd_aggregate.h"


// Returns the minimum number of lamports required to make an account
// with dlen bytes of data rent exempt. These values were calculated
// using the getMinimumBalanceForRentExemption RPC call, and are
// guaranteed never to increase.
static uint64_t rent_exempt_amount( uint64_t dlen )
{
  switch ( dlen )
  {
  case sizeof( pc_map_table_t ):
    return 143821440;
  case PC_PROD_ACC_SIZE:
    return 4454400;
  case sizeof( pc_price_t ):
    return 23942400;
  case PRICE_ACCOUNT_SIZE:
    return 36915840;
  default:
    return UINT64_MAX;
  }
}

static bool is_rent_exempt( uint64_t lamports, uint64_t dlen )
{
  return lamports >= rent_exempt_amount( dlen );
}

static bool valid_funding_account( SolAccountInfo *ka )
{
  return ka->is_signer &&
         ka->is_writable;
}

static bool valid_signable_account( SolParameters *prm,
                                    SolAccountInfo *ka,
                                    uint64_t min_dlen )
{
  return ka->is_signer &&
         ka->is_writable &&
         SolPubkey_same( ka->owner, prm->program_id ) &&
         ka->data_len >= min_dlen &&
         is_rent_exempt( *ka->lamports, ka->data_len );
}

static bool valid_writable_account( SolParameters *prm,
                                    SolAccountInfo *ka,
                                    uint64_t min_dlen )
{
  return ka->is_writable &&
         SolPubkey_same( ka->owner, prm->program_id ) &&
         ka->data_len >= min_dlen &&
         is_rent_exempt( *ka->lamports, ka->data_len );
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
  sptr->prev_timestamp_ = 0L;
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
  if ( ( cptr->cmd_ == e_cmd_upd_price || cptr->cmd_ == e_cmd_upd_price_no_fail_on_error ) &&
       cptr->pub_slot_ <= fptr->pub_slot_ ) {
    return ERROR_INVALID_ARGUMENT;
  }
  bool updated_aggregate = false;
  // update aggregate price as necessary
  if ( sptr->slot_ > pptr->agg_.pub_slot_ ) {
    updated_aggregate = upd_aggregate( pptr, sptr->slot_, sptr->unix_timestamp_ );
  }

  // update component price if required
  if ( cptr->cmd_ == e_cmd_upd_price || cptr->cmd_ == e_cmd_upd_price_no_fail_on_error ) {
    uint32_t status = cptr->status_;

    // Set publisher's status to unknown unless their CI is sufficiently tight.
    int64_t threshold_conf = (cptr->price_ / PC_MAX_CI_DIVISOR);
    if (threshold_conf < 0) {
      // Safe as long as threshold_conf isn't the min int64, which it isn't as long as PC_MAX_CI_DIVISOR > 1.
      threshold_conf = -threshold_conf;
    }
    if ( cptr->conf_ > (uint64_t) threshold_conf ) {
      status = PC_STATUS_UNKNOWN;
    }

    fptr->price_    = cptr->price_;
    fptr->conf_     = cptr->conf_;
    fptr->status_   = status;
    fptr->pub_slot_ = cptr->pub_slot_;
  }
  if (updated_aggregate){
    return SUCCESSFULLY_UPDATED_AGGREGATE;
  }
  return SUCCESS;
}

static uint64_t upd_price_no_fail_on_error( SolParameters *prm, SolAccountInfo *ka )
{
  return upd_price( prm, ka ) == SUCCESSFULLY_UPDATED_AGGREGATE? SUCCESSFULLY_UPDATED_AGGREGATE : SUCCESS;
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
    case e_cmd_agg_price:                  return upd_price( prm, ka );
    case e_cmd_upd_price_no_fail_on_error: return upd_price_no_fail_on_error( prm, ka );
    case e_cmd_init_mapping:               return ERROR_INVALID_ARGUMENT;
    case e_cmd_add_mapping:                return ERROR_INVALID_ARGUMENT;
    case e_cmd_add_product:                return ERROR_INVALID_ARGUMENT;
    case e_cmd_upd_product:                return ERROR_INVALID_ARGUMENT;
    case e_cmd_add_price:                  return ERROR_INVALID_ARGUMENT;
    case e_cmd_add_publisher:              return ERROR_INVALID_ARGUMENT;
    case e_cmd_del_publisher:              return del_publisher( prm, ka );
    case e_cmd_init_price:                 return init_price( prm, ka );
    case e_cmd_init_test:                  return ERROR_INVALID_ARGUMENT;
    case e_cmd_upd_test:                   return ERROR_INVALID_ARGUMENT;
    case e_cmd_set_min_pub:                return ERROR_INVALID_ARGUMENT;
    default:                               return ERROR_INVALID_ARGUMENT;
  }
}

extern uint64_t c_entrypoint(const uint8_t *input)
{
  SolAccountInfo ka[4];
  SolParameters prm[1];
  prm->ka = ka;
  if (!sol_deserialize(input, prm, SOL_ARRAY_SIZE(ka))) {
    return ERROR_INVALID_ARGUMENT;
  }
  return dispatch( prm, ka );
}
