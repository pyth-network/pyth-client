/*
 * BPF pyth price oracle program
 */
#include <solana_sdk.h>
#include "oracle.h"

typedef enum {

  /*
   * initialize mapping account data
   * key[0] funding account [signer]
   * key[1] mapping account [signer]
   */
  e_init_mapping = 0,

  /*
   * add symbol/account to mapping and initialize account data
   * key[0] funding account [signer]
   * key[1] mapping account [signer writable]
   * key[2] symbol account  [signer writable]
   */
  e_add_symbol_mapping

} command_t;

/*
 * parameters for e_add_symbol_mapping command
 */
typedef struct cmd_add_symbol_mapping
{
  int32_t     cmd_;
  pc_symbol_t symbol_;
  uint64_t    ndp_;
} cmd_add_symbol_mapping_t;

uint64_t init_mapping( SolParameters *prm, SolAccountInfo *ka )
{
  if ( prm->ka_num != 2 ) {
    return ERROR_INVALID_ARGUMENT;
  }
  if ( !ka[0].is_signer ||
       !ka[1].is_signer ||
       !ka[1].is_writable ||
       !SolPubkey_same( ka[1].owner, prm->program_id ) ||
       ka[1].data_len != sizeof( pc_map_table_t ) ) {
    return ERROR_INVALID_ARGUMENT;
  }
  sol_memset( ka[1].data, 0, sizeof( pc_map_table_t ) );

  return PC_RETURN_SUCCESS;
}

uint64_t add_symbol_mapping( SolParameters *prm, SolAccountInfo *ka )
{
  if ( prm->ka_num != 3 ) {
    return ERROR_INVALID_ARGUMENT;
  }
  if ( !ka[0].is_signer ||
       !ka[1].is_signer ||
       !ka[1].is_writable ||
       !ka[2].is_signer ||
       !ka[2].is_writable ||
       !SolPubkey_same( ka[1].owner, prm->program_id ) ||
       !SolPubkey_same( ka[2].owner, prm->program_id ) ||
       ka[1].data_len != sizeof( pc_map_table_t ) ||
       ka[2].data_len != sizeof( pc_price_node_t ) ||
       prm->data_len != sizeof( cmd_add_symbol_mapping_t ) ) {
    return ERROR_INVALID_ARGUMENT;
  }
  cmd_add_symbol_mapping_t *cp = (cmd_add_symbol_mapping_t*)prm->data;
  pc_symbol_t *sym = &cp->symbol_;
  pc_map_table_t *tptr = (pc_map_table_t*)ka[1].data;
  pc_pub_key_t *acc = (pc_pub_key_t*)ka[2].key;
  uint64_t res = pc_map_table_add_symbol( tptr, sym, acc, cp->ndp_ );
  if ( res == PC_RETURN_SUCCESS ) {
    pc_price_node_t *pptr = (pc_price_node_t*)ka[2].data;
    sol_memset( pptr, 0, sizeof( pc_price_node_t ) );
    pc_symbol_assign( &pptr->sym_, sym );
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
  if (prm->data_len < sizeof(int32_t) ) {
    return ERROR_INVALID_ARGUMENT;
  }
  uint32_t cmd = *(uint32_t*)prm->data;
  uint64_t res = ERROR_INVALID_ARGUMENT;
  switch(cmd) {
    case e_init_mapping:{
      res = init_mapping( prm, ka );
      break;
    }
    case e_add_symbol_mapping:{
      res = add_symbol_mapping( prm, ka );
      break;
    }
    default: break;
  }
  return res;
}
