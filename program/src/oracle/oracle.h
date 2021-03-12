#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// magic number at head of account
#define PC_MAGIC 0xa1b2c3d4

// versions available
#define PC_VERSION_1         1U

// latest program version
#define PC_VERSION           PC_VERSION_1

// various array sizes
#define PC_SYMBOL_SIZE       16
#define PC_PUBKEY_SIZE       32
#define PC_SYMBOL_SIZE_64   (PC_SYMBOL_SIZE/sizeof(uint64_t))
#define PC_PUBKEY_SIZE_64   (PC_PUBKEY_SIZE/sizeof(uint64_t))
#define PC_MAP_TABLE_SIZE   307
#define PC_MAP_NODE_SIZE    300
#define PC_COMP_SIZE         16
#define PC_MAX_NUM_DECIMALS  16

// price types
#define PC_PTYPE_UNKNOWN      0
#define PC_PTYPE_PRICE        1
#define PC_PTYPE_TWAP         2

// symbol status
#define PC_STATUS_UNKNOWN     0
#define PC_STATUS_TRADING     1
#define PC_STATUS_HALTED      2

// binary version of sysvar_clock account id
const uint64_t sysvar_clock[] = {
  0xc974c71817d5a706UL,
  0xb65e1d6998635628UL,
  0x5c6d4b9ba3b85e8bUL,
  0x215b5573UL
};

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

// individual symbol/price account mapping
typedef struct pc_map_node
{
  uint32_t        next_;
  uint32_t        unused_;
  pc_symbol_t     sym_;              // symbol name
  pc_pub_key_t    price_acc_;        // first price account in chain
} pc_map_node_t;

// hash table of symbol to price account mappings
typedef struct pc_map_table
{
  uint32_t        magic_;            // pyth magic number
  uint32_t        ver_;              // program/account version
  uint32_t        num_;              // number of symbols
  uint32_t        tab_[PC_MAP_TABLE_SIZE];
  pc_map_node_t   nds_[PC_MAP_NODE_SIZE];
  pc_pub_key_t    next_;             // next mapping account in chain
} pc_map_table_t;

// price et al. for some component or aggregate
typedef struct pc_price_info
{
  int64_t         price_;            // price per ptype_
  uint64_t        conf_;             // price confidence interval
  uint32_t        status_;           // symbol status as of last update
  uint32_t        unused_;           // 64bit padding
  uint64_t        pub_slot_;         // publish slot-time of price
} pc_price_info_t;

// published component price for contributing provider
typedef struct pc_price_comp
{
  pc_pub_key_t    pub_;              // publishing key of component price
  pc_price_info_t agg_;              // price used in aggregate calc
  pc_price_info_t latest_;           // latest contributed prices
} pc_price_comp_t;

// price account containing aggregate and all component prices
typedef struct pc_price
{
  uint32_t        magic_;             // pyth magic number
  uint32_t        ver_;               // program version
  uint32_t        unused_;            // 64bit padding
  uint32_t        ptype_;             // price or calculation type
  int32_t         expo_;              // price exponent
  uint32_t        num_;               // number of component prices
  uint64_t        curr_slot_;         // currently accumulating price slot
  uint64_t        valid_slot_;        // valid slot-time of agg. price
  pc_symbol_t     sym_;               // symbol for this account
  pc_pub_key_t    agg_pub_;           // aggregate price updater
  pc_price_info_t agg_;               // aggregate price information
  pc_price_comp_t comp_[PC_COMP_SIZE];// component prices
  pc_pub_key_t    next_;              // next price account
} pc_price_t;

// command enumeration
typedef enum {

  // initialize first mapping list account
  // key[0] funding account       [signer writable]
  // key[1] mapping account       [signer writable]
  e_cmd_init_mapping = 0,

  // initialize and add new mapping account
  // key[0] funding account       [signer writable]
  // key[1] tail mapping account  [signer writable]
  // key[2] new mapping account   [signer writable]
  e_cmd_add_mapping,

  // add new symbol price account version node to
  // key[0] funding account       [signer writable]
  // key[1] mapping account       [signer writable]
  // key[2] new price account     [signer writable]
  e_cmd_add_symbol,

  // add publisher to symbol account
  // key[0] funding account       [signer writable]
  // key[1] price account         [signer writable]
  e_cmd_add_publisher,

  // delete publisher from symbol account
  // key[0] funding account       [signer writable]
  // key[1] price account         [signer writable]
  e_cmd_del_publisher,

  // publish component price
  // key[0] funding account       [signer writable]
  // key[1] price account         [writable]
  // key[2] sysvar_clock account  [readable]
  e_cmd_upd_price

} command_t;

typedef struct cmd_hdr
{
  uint32_t     ver_;
  int32_t      cmd_;
} cmd_hdr_t;

typedef struct cmd_add_symbol
{
  uint32_t     ver_;
  int32_t      cmd_;
  int32_t      expo_;
  uint32_t     ptype_;
  pc_symbol_t  sym_;
} cmd_add_symbol_t;

typedef struct cmd_add_publisher
{
  uint32_t     ver_;
  int32_t      cmd_;
  uint32_t     ptype_;
  uint32_t     unused_;
  pc_symbol_t  sym_;
  pc_pub_key_t pub_;
} cmd_add_publisher_t;

typedef struct cmd_del_publisher
{
  uint32_t     ver_;
  int32_t      cmd_;
  pc_symbol_t  sym_;
  pc_pub_key_t pub_;
} cmd_del_publisher_t;

typedef struct cmd_upd_price
{
  uint32_t     ver_;
  int32_t      cmd_;
  uint32_t     ptype_;
  uint32_t     status_;
  pc_symbol_t  sym_;
  int64_t      price_;
  uint64_t     conf_;
} cmd_upd_price_t;

// structure of clock sysvar account
typedef struct sysvar_clock
{
  uint64_t    slot_;
  int64_t     epoch_start_timestamp_;
  uint64_t    epoch_;
  uint64_t    leader_schedule_epoch_;
  int64_t     unix_timestamp_;
} sysvar_clock_t;

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
