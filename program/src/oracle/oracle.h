#pragma once

#ifdef __cplusplus
extern "C" {
#else
#include <stdbool.h>
#endif

// magic number at head of account
#define PC_MAGIC 0xa1b2c3d4

// current program version
#define PC_VERSION            2

// max latency in slots between send and receive
#define PC_MAX_SEND_LATENCY  25

// various size constants
#define PC_PUBKEY_SIZE       32
#define PC_PUBKEY_SIZE_64   (PC_PUBKEY_SIZE/sizeof(uint64_t))
#define PC_MAP_TABLE_SIZE   640
#define PC_COMP_SIZE         32
#define PC_MAX_NUM_DECIMALS  16
#define PC_PROD_ACC_SIZE    512
#define PC_EXP_DECAY       (-9)

#ifndef PC_HEAP_START
#define PC_HEAP_START (0x300000000)
#endif

// price types
#define PC_PTYPE_UNKNOWN      0
#define PC_PTYPE_PRICE        1

// symbol status
#define PC_STATUS_UNKNOWN     0
#define PC_STATUS_TRADING     1
#define PC_STATUS_HALTED      2
#define PC_STATUS_AUCTION     3

// account types
#define PC_ACCTYPE_MAPPING    1
#define PC_ACCTYPE_PRODUCT    2
#define PC_ACCTYPE_PRICE      3
#define PC_ACCTYPE_TEST       4

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

static_assert( sizeof( pc_pub_key_t ) == 32, "" );

// account header information
typedef struct pc_acc
{
  uint32_t        magic_;            // pyth magic number
  uint32_t        ver_;              // program/account version
  uint32_t        type_;             // account type
  uint32_t        size_;             // size of populated region of account
} pc_acc_t;

static_assert( sizeof( pc_acc_t ) == 16, "" );

// hash table of symbol to price account mappings
typedef struct pc_map_table
{
  uint32_t        magic_;            // pyth magic number
  uint32_t        ver_;              // program/account version
  uint32_t        type_;             // account type
  uint32_t        size_;             // size of populated region of account
  uint32_t        num_;              // number of symbols
  uint32_t        unused_;           // 64bit padding
  pc_pub_key_t    next_;             // next mapping account in chain
  pc_pub_key_t    prod_[PC_MAP_TABLE_SIZE]; // product accounts
} pc_map_table_t;

static_assert( sizeof( pc_map_table_t ) == 20536, "" );

// variable length string
typedef struct pc_str
{
  uint8_t         len_;
  char            data_[];
} pc_str_t;

static_assert( sizeof( pc_str_t ) == 1, "" );

// product reference data
typedef struct pc_prod
{
  uint32_t        magic_;
  uint32_t        ver_;              // program version
  uint32_t        type_;             // account type
  uint32_t        size_;             // size of populated region of account
  pc_pub_key_t    px_acc_;           // first price (or quote) account
  // variable number of reference key/value attribute pairs
  // stored as strings (pc_str_t)
} pc_prod_t;

static_assert( sizeof( pc_prod_t ) == 48, "" );

// price et al. for some component or aggregate
typedef struct pc_price_info
{
  int64_t         price_;            // price per ptype_
  uint64_t        conf_;             // price confidence interval
  uint32_t        status_;           // symbol status as of last update
  uint32_t        corp_act_status_;  // corp action status as of last update
  uint64_t        pub_slot_;         // publish slot of price
} pc_price_info_t;

static_assert( sizeof( pc_price_info_t ) == 32, "" );

// published component price for contributing provider
typedef struct pc_price_comp
{
  pc_pub_key_t    pub_;              // publishing key of component price
  pc_price_info_t agg_;              // price used in aggregate calc
  pc_price_info_t latest_;           // latest contributed prices
} pc_price_comp_t;

static_assert( sizeof( pc_price_comp_t ) == 96, "" );

// time-weighted exponential moving average
typedef struct pc_ema
{
  int64_t val_;                       // current value of ema
  int64_t numer_;                     // numerator at full precision
  int64_t denom_;                     // denominator at full precision
} pc_ema_t;

static_assert( sizeof( pc_ema_t ) == 24, "" );

// price account containing aggregate and all component prices
typedef struct pc_price
{
  uint32_t        magic_;             // pyth magic number
  uint32_t        ver_;               // program version
  uint32_t        type_;              // account type
  uint32_t        size_;              // price account size
  uint32_t        ptype_;             // price or calculation type
  int32_t         expo_;              // price exponent
  uint32_t        num_;               // number of component prices
  uint32_t        num_qt_;            // number of quoters that make up aggregate
  uint64_t        last_slot_;         // slot of last valid aggregate price
  uint64_t        valid_slot_;        // valid on-chain slot of agg. price
  pc_ema_t        twap_;              // time-weighted average price
  pc_ema_t        twac_;              // time-weighted average conf interval
  int64_t         drv1_;              // space for future derived values
  uint8_t         min_pub_;           // min publishers for valid price
  int8_t          drv2_;              // space for future derived values
  int16_t         drv3_;              // space for future derived values
  int32_t         drv4_;              // space for future derived values
  pc_pub_key_t    prod_;              // product id/ref-account
  pc_pub_key_t    next_;              // next price account in list
  uint64_t        prev_slot_;         // valid slot of previous update
  int64_t         prev_price_;        // aggregate price of previous update
  uint64_t        prev_conf_;         // confidence interval of previous update
  uint64_t        drv5_;              // space for future derived values
  pc_price_info_t agg_;               // aggregate price information
  pc_price_comp_t comp_[PC_COMP_SIZE];// component prices
} pc_price_t;

static_assert( sizeof( pc_price_t ) == 3312, "" );

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

  // initialize and add new product reference data account
  // key[0] funding account       [signer writable]
  // key[1] mapping account       [signer writable]
  // key[2] new product account   [signer writable]
  e_cmd_add_product,

  // update product account
  // key[0] funding account       [signer writable]
  // key[1] product account       [signer writable]
  e_cmd_upd_product,

  // add new price account to a product account
  // key[0] funding account       [signer writable]
  // key[1] product account       [signer writable]
  // key[2] new price account     [signer writable]
  e_cmd_add_price,

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
  e_cmd_upd_price,

  // compute aggregate price
  // key[0] funding account       [signer writable]
  // key[1] price account         [writable]
  // key[2] sysvar_clock account  [readable]
  e_cmd_agg_price,

  // (re)initialize price account
  // key[0] funding account       [signer writable]
  // key[1] new price account     [signer writable]
  e_cmd_init_price,

  // initialize test account
  // key[0] funding account       [signer writable]
  // key[1] test account          [signer writable]
  e_cmd_init_test,

  // run aggregate price test
  // key[0] funding account       [signer writable]
  // key[1] test account          [signer writable]
  e_cmd_upd_test,

  // set min publishers
  // key[0] funding account       [signer writable]
  // key[1] price account         [signer writable]
  e_cmd_set_min_pub

} command_t;

typedef struct cmd_hdr
{
  uint32_t     ver_;
  int32_t      cmd_;
} cmd_hdr_t;

static_assert( sizeof( cmd_hdr_t ) == 8, "" );

typedef struct cmd_add_product
{
  uint32_t     ver_;
  int32_t      cmd_;
} cmd_add_product_t;

static_assert( sizeof( cmd_add_product_t ) == 8, "" );

typedef struct cmd_upd_product
{
  uint32_t     ver_;
  int32_t      cmd_;
  // set of key-value pairs
} cmd_upd_product_t;

static_assert( sizeof( cmd_upd_product_t ) == 8, "" );

typedef struct cmd_add_price
{
  uint32_t     ver_;
  int32_t      cmd_;
  int32_t      expo_;
  uint32_t     ptype_;
} cmd_add_price_t;

static_assert( sizeof( cmd_add_price_t ) == 16, "" );

typedef struct cmd_init_price
{
  uint32_t     ver_;
  int32_t      cmd_;
  int32_t      expo_;
  uint32_t     ptype_;
} cmd_init_price_t;

static_assert( sizeof( cmd_init_price_t ) == 16, "" );

typedef struct cmd_set_min_pub
{
  uint32_t     ver_;
  int32_t      cmd_;
  uint8_t      min_pub_;
} cmd_set_min_pub_t;

static_assert( sizeof( cmd_set_min_pub_t ) == 12, "" );

typedef struct cmd_add_publisher
{
  uint32_t     ver_;
  int32_t      cmd_;
  pc_pub_key_t pub_;
} cmd_add_publisher_t;

static_assert( sizeof( cmd_add_publisher_t ) == 40, "" );

typedef struct cmd_del_publisher
{
  uint32_t     ver_;
  int32_t      cmd_;
  pc_pub_key_t pub_;
} cmd_del_publisher_t;

static_assert( sizeof( cmd_del_publisher_t ) == 40, "" );

typedef struct cmd_upd_price
{
  uint32_t     ver_;
  int32_t      cmd_;
  uint32_t     status_;
  uint32_t     unused_;
  int64_t      price_;
  uint64_t     conf_;
  uint64_t     pub_slot_;
} cmd_upd_price_t;

static_assert( sizeof( cmd_upd_price_t ) == 40, "" );

typedef struct cmd_upd_test
{
  uint32_t     ver_;
  int32_t      cmd_;
  uint32_t     num_;
  int32_t      expo_;
  int8_t       slot_diff_[PC_COMP_SIZE];
  int64_t      price_[PC_COMP_SIZE];
  uint64_t     conf_[PC_COMP_SIZE];
} cmd_upd_test_t;

static_assert( sizeof( cmd_upd_test_t ) == 560, "" );

// structure of clock sysvar account
typedef struct sysvar_clock
{
  uint64_t    slot_;
  int64_t     epoch_start_timestamp_;
  uint64_t    epoch_;
  uint64_t    leader_schedule_epoch_;
  int64_t     unix_timestamp_;
} sysvar_clock_t;

static_assert( sizeof( sysvar_clock_t ) == 40, "" );

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
