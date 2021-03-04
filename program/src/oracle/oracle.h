#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// latest program version
#define PC_VERSION           1U

// return codes
#define PC_RETURN_SUCCESS            0
#define PC_RETURN_SYMBOL_EXISTS      1
#define PC_RETURN_SYMBOL_NOT_EXISTS  2
#define PC_RETURN_MAX_CAPACITY       3
#define PC_RETURN_PUBKEY_EXISTS      4
#define PC_RETURN_PUBKEY_NOT_EXISTS  5
#define PC_RETURN_ACCOUNT_EXISTS     6
#define PC_RETURN_ACCOUNT_NOT_EXISTS 7

// various array sizes
#define PC_SYMBOL_SIZE       16
#define PC_PUBKEY_SIZE       32
#define PC_SYMBOL_SIZE_64   (PC_SYMBOL_SIZE/sizeof(uint64_t))
#define PC_PUBKEY_SIZE_64   (PC_PUBKEY_SIZE/sizeof(uint64_t))
#define PC_MAP_TABLE_SIZE   307
#define PC_MAP_NODE_SIZE    300
#define PC_COMP_SIZE         16
#define PC_MAX_NUM_DECIMALS  16

// invalid price used when a new publisher is added to
// a symbol account but not yet published a price
#define PC_INVALID_PRICE    -999999999999999L

// public key of symbol or publisher account
typedef union pc_pub_key
{
  uint8_t  k1_[PC_PUBKEY_SIZE];
  uint64_t k8_[PC_PUBKEY_SIZE_64];
} pc_pub_key_t;

// asset symbol name
typedef union pc_symbol
{
  int8_t   k1_[PC_SYMBOL_SIZE];
  uint64_t k8_[PC_SYMBOL_SIZE_64];
} pc_symbol_t;

// mapping account contains mapping of symbol to account number
// and list of authorized component publishers
typedef struct pc_map_node
{
  uint32_t      next_;
  uint32_t      pad_;
  pc_symbol_t   sym_;
  pc_pub_key_t  acc_;
} pc_map_node_t;

typedef struct pc_map_table
{
  uint32_t      ver_;
  uint32_t      num_;
  uint32_t      pad_;
  uint32_t      tab_[PC_MAP_TABLE_SIZE];
  pc_map_node_t nds_[PC_MAP_NODE_SIZE];
  pc_pub_key_t  prev_;
  pc_pub_key_t  next_;
} pc_map_table_t;

// symbol account contains component and aggregate prices
typedef struct pc_price
{
  pc_pub_key_t  pub_;
  int64_t       price_;
  uint64_t      conf_;
} pc_price_t;

typedef struct pc_price_node
{
  uint32_t      ver_;
  uint32_t      pad_;
  pc_symbol_t   sym_;
  int32_t       expo_;
  uint32_t      num_;
  pc_price_t    agg_;
  pc_price_t    comp_[PC_COMP_SIZE];
} pc_price_node_t;

// command enumeration
typedef enum {

  // initialize first mapping account
  // key[0] funding account       [signer writable]
  // key[1] mapping account       [signer writable]
  e_cmd_init_mapping = 0,

  // initialize and add new mapping account
  // key[0] funding account       [signer writable]
  // key[1] tail mapping account  [signer writable]
  // key[2] new mapping account   [signer writable]
  e_cmd_add_mapping,

  // add symbol account to mapping account and initialize
  // key[0] funding account       [signer writable]
  // key[1] mapping account       [signer writable]
  // key[2] symbol account        [signer writable]
  e_cmd_add_symbol,

  // add publisher to symbol account
  // key[0] funding account       [signer writable]
  // key[1] symbol account        [signer writable]
  e_cmd_add_publisher,

  // delete publisher from symbol account
  // key[0] funding account       [signer writable]
  // key[1] symbol account        [signer writable]
  e_cmd_del_publisher,

  // update component price to symbol account
  // key[0] funding account       [signer writable]
  // key[1] symbol account        [writable]
  e_cmd_upd_price,

  // calc aggregate price for symbol
  // key[0] publisher account     [signer writable]
  // key[1] symbol account        [writable]
  e_cmd_agg_price

} command_t;

// parameters for commands
typedef struct cmd_add_symbol
{
  int32_t     cmd_;
  int32_t     expo_;
  pc_symbol_t sym_;
} cmd_add_symbol_t;

typedef struct cmd_add_publisher
{
  int32_t      cmd_;
  uint32_t     pad_;
  pc_symbol_t  sym_;
  pc_pub_key_t pub_;
} cmd_add_publisher_t;

typedef struct cmd_del_publisher
{
  int32_t      cmd_;
  uint32_t     pad_;
  pc_symbol_t  sym_;
  pc_pub_key_t pub_;
} cmd_del_publisher_t;

typedef struct cmd_upd_price
{
  int32_t     cmd_;
  uint32_t    pad_;
  pc_symbol_t sym_;
  int64_t     price_;
  uint64_t    conf_;
} cmd_upd_price_t;

// compare if two symbols are the same
inline bool pc_symbol_equal( pc_symbol_t *s1, pc_symbol_t *s2 )
{
  return s1->k8_[0] == s2->k8_[0] && s1->k8_[1] == s2->k8_[1];
}

// check for null (zero) symbol key
inline bool pc_symbol_is_zero( pc_symbol_t *s )
{
  return s->k8_[0] == 0UL && s->k8_[1] == 0UL;
}

// assign one symbol from another
inline void pc_symbol_assign( pc_symbol_t *tgt, pc_symbol_t *src )
{
  tgt->k8_[0] = src->k8_[0];
  tgt->k8_[1] = src->k8_[1];
}

// compare if two pub_keys (accounts) are the same
inline bool pc_pub_key_equal( pc_pub_key_t *p1, pc_pub_key_t *p2 )
{
  return p1->k8_[0] == p2->k8_[0] &&
         p1->k8_[1] == p2->k8_[1] &&
         p1->k8_[2] == p2->k8_[2] &&
         p1->k8_[3] == p2->k8_[3];
}

// check for null (zero) public key
inline bool pc_pub_key_is_zero( pc_pub_key_t *p )
{
  return p->k8_[0] == 0UL &&
         p->k8_[1] == 0UL &&
         p->k8_[2] == 0UL &&
         p->k8_[3] == 0UL;
}

// set public key to zero
inline void pc_pub_key_set_zero( pc_pub_key_t *p )
{
  p->k8_[0] = p->k8_[1] = p->k8_[2] = p->k8_[3] = 0UL;
}

// assign one pub_key from another
inline void pc_pub_key_assign( pc_pub_key_t *tgt, pc_pub_key_t *src )
{
  tgt->k8_[0] = src->k8_[0];
  tgt->k8_[1] = src->k8_[1];
  tgt->k8_[2] = src->k8_[2];
  tgt->k8_[3] = src->k8_[3];
}


#ifdef __cplusplus
}
#endif
