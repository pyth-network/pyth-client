#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#define PC_RETURN_SUCCESS            0
#define PC_RETURN_SYMBOL_EXISTS      1
#define PC_RETURN_SYMBOL_NOT_EXISTS  2
#define PC_RETURN_MAX_CAPACITY       3
#define PC_RETURN_PUBKEY_EXISTS      4
#define PC_RETURN_PUBKEY_NOT_EXISTS  5
#define PC_RETURN_ACCOUNT_NOT_EXISTS 6

#define PC_SYMBOL_SIZE    16
#define PC_PUBKEY_SIZE    32
#define PC_SYMBOL_SIZE_64 (PC_SYMBOL_SIZE/sizeof(p_uint64_t))
#define PC_PUBKEY_SIZE_64 (PC_PUBKEY_SIZE/sizeof(p_uint64_t))
#define PC_MAP_TABLE_SIZE 197
#define PC_MAP_NODE_SIZE  300
#define PC_COMP_SIZE      8

typedef char           p_int8_t;
typedef unsigned char  p_uint8_t;
typedef unsigned short p_uint16_t;
typedef unsigned int   p_uint32_t;
typedef unsigned long  p_uint64_t;

/*
 * mapping account contains mapping of symbol to account number
 * and list of authorized component publishers
 */

typedef union pc_pub_key
{
  p_uint8_t  k1_[PC_PUBKEY_SIZE];
  p_uint64_t k8_[PC_PUBKEY_SIZE_64];
} pc_pub_key_t;

typedef union pc_symbol
{
  p_int8_t   k1_[PC_SYMBOL_SIZE];
  p_uint64_t k8_[PC_SYMBOL_SIZE_64];
} pc_symbol_t;

typedef struct pc_map_node
{
  p_uint16_t next_;
  union {
    struct {
      p_uint16_t   acc_;
      p_uint16_t   pub_;
      p_uint32_t   ndp_;
      pc_symbol_t  sym_;
    } sym_;
    pc_pub_key_t acc_;
    pc_pub_key_t pub_;
  };
} pc_map_node_t;

typedef struct pc_map_table
{
  p_uint16_t    num_;
  p_uint16_t    reuse_;
  p_uint16_t    tab_[PC_MAP_TABLE_SIZE];
  pc_map_node_t nds_[PC_MAP_NODE_SIZE];
} pc_map_table_t;

/*
 * symbol account contains component and aggregate prices
 */

typedef struct pc_price
{
  pc_pub_key_t pub_;
  p_uint64_t   price_;
  p_uint64_t   conf_;
} pc_price_t;

typedef struct pc_price_node
{
  pc_symbol_t sym_;
  pc_price_t  agg_;
  pc_price_t  comp_[PC_COMP_SIZE];
} pc_price_node_t;

/*
 * compare if two symbols are the same
 */
bool pc_symbol_equal( pc_symbol_t *s1, pc_symbol_t *s2 )
{
  return s1->k8_[0] == s2->k8_[0] && s1->k8_[1] == s2->k8_[1];
}

/*
 * assign one symbol from another
 */
void pc_symbol_assign( pc_symbol_t *tgt, pc_symbol_t *src )
{
  tgt->k8_[0] = src->k8_[0];
  tgt->k8_[1] = src->k8_[1];
}

/*
 * compare if two pub_keys (accounts) are the same
 */
bool pc_pub_key_equal( pc_pub_key_t *p1, pc_pub_key_t *p2 )
{
  return p1->k8_[0] == p2->k8_[0] &&
         p1->k8_[1] == p2->k8_[1] &&
         p1->k8_[2] == p2->k8_[2] &&
         p1->k8_[3] == p2->k8_[3];
}

/*
 * assign one pub_key from another
 */
void pc_pub_key_assign( pc_pub_key_t *tgt, pc_pub_key_t *src )
{
  tgt->k8_[0] = src->k8_[0];
  tgt->k8_[1] = src->k8_[1];
  tgt->k8_[2] = src->k8_[2];
  tgt->k8_[3] = src->k8_[3];
}

/*
 * find node reference corresponding to symbol
 */
p_uint16_t *pc_map_table_find_node( pc_map_table_t *tptr,
                                    pc_symbol_t *sptr )
{
  p_uint16_t idx = sptr->k8_[0] % (p_uint64_t)PC_MAP_TABLE_SIZE;
  for( p_uint16_t *vptr = &tptr->tab_[idx]; *vptr; ) {
    pc_map_node_t *nptr = &tptr->nds_[*vptr];
    if ( pc_symbol_equal( sptr, &nptr->sym_.sym_ ) ) {
      return vptr;
    }
    vptr = &nptr->next_;
  }
  return (p_uint16_t*)0;
}

/*
 * find node reference corresponding to symbol/publisher
 */
p_uint16_t *pc_map_table_find_publisher( pc_map_table_t *tptr,
                                         pc_map_node_t *nptr,
                                         pc_pub_key_t *pptr )
{
  for( p_uint16_t *vptr = &nptr->sym_.pub_; *vptr; ) {
    pc_map_node_t *iptr = &tptr->nds_[*vptr];
    if ( pc_pub_key_equal( pptr, &iptr->pub_ ) ) {
      return vptr;
    }
    vptr = &iptr->next_;
  }
  return (p_uint16_t*)0;
}

/*
 * get symbol account from symbol
 */

pc_pub_key_t *pc_map_table_get_account( pc_map_table_t *tptr,
                                        pc_symbol_t *sptr )
{
  p_uint16_t *vptr = pc_map_table_find_node( tptr, sptr );
  if ( !vptr ) {
    return (pc_pub_key_t*)0;
  }
  pc_map_node_t *nptr = &tptr->nds_[*vptr];
  pc_map_node_t *an = &tptr->nds_[nptr->sym_.acc_];
  return &an->acc_;
}

/*
 * allocate new node in mapping table
 */
p_uint16_t pc_map_table_alloc_node( pc_map_table_t *tptr )
{
  p_uint16_t ridx = tptr->reuse_;
  if ( ridx ) {
    tptr->reuse_ = tptr->nds_[ridx].next_;
    return ridx;
  }
  if ( tptr->num_ == PC_MAP_NODE_SIZE ) {
    return 0;
  }
  return ++tptr->num_;
}

/*
 * deallocate node in mapping table
 */
void pc_map_table_dealloc_node( pc_map_table_t *tptr, p_uint16_t ridx )
{
  tptr->nds_[ridx].next_ = tptr->reuse_;
  tptr->reuse_ = ridx;
}

/*
 * add new symbol/account in mapping table
 */
p_uint64_t pc_map_table_add_symbol( pc_map_table_t *tptr,
                                    pc_symbol_t *sptr,
                                    pc_pub_key_t *aptr,
                                    p_uint64_t ndp )
{
  p_uint16_t idx = sptr->k8_[0] % (p_uint64_t)PC_MAP_TABLE_SIZE;
  for( p_uint16_t vidx = tptr->tab_[idx]; vidx; ) {
    pc_map_node_t *nptr = &tptr->nds_[vidx];
    vidx = nptr->next_;
    if ( pc_symbol_equal( sptr, &nptr->sym_.sym_ ) ) {
      return PC_RETURN_SYMBOL_EXISTS;
    }
  }
  p_uint16_t hidx = pc_map_table_alloc_node( tptr );
  if ( hidx == 0 ) {
    return PC_RETURN_MAX_CAPACITY;
  }
  p_uint16_t aidx = pc_map_table_alloc_node( tptr );
  if ( aidx == 0 ) {
    return PC_RETURN_MAX_CAPACITY;
  }
  pc_map_node_t *hn = &tptr->nds_[hidx];
  hn->next_ = tptr->tab_[idx];
  hn->sym_.acc_ = aidx;
  hn->sym_.pub_ = 0;
  hn->sym_.ndp_ = ndp;
  pc_symbol_assign( &hn->sym_.sym_, sptr );
  tptr->tab_[idx] = hidx;

  pc_map_node_t *an = &tptr->nds_[aidx];
  an->next_ = 0;
  pc_pub_key_assign( &an->acc_, aptr );
  return PC_RETURN_SUCCESS;
}

/*
 * remove symbol and corresponding publishing keys from mapping table
 */
p_uint64_t pc_map_table_del_symbol( pc_map_table_t *tptr,
                                    pc_symbol_t *sptr )
{
  p_uint16_t *vptr = pc_map_table_find_node( tptr, sptr );
  if ( !vptr ) {
    return PC_RETURN_SYMBOL_NOT_EXISTS;
  }
  p_uint16_t hidx = *vptr;
  pc_map_node_t *hn = &tptr->nds_[hidx];
  *vptr = hn->next_;
  for( p_uint16_t pidx = hn->sym_.pub_; pidx; ) {
    p_uint16_t nidx = tptr->nds_[pidx].next_;
    pc_map_table_dealloc_node( tptr, pidx );
    pidx = nidx;
  }
  pc_map_table_dealloc_node( tptr, hn->sym_.acc_ );
  pc_map_table_dealloc_node( tptr, hidx );
  return PC_RETURN_SUCCESS;
}

/*
 * add publisher key to symbol account
 */

p_uint64_t pc_map_table_add_publisher( pc_map_table_t *tptr,
                                       pc_symbol_t *sptr,
                                       pc_pub_key_t *pptr )
{
  p_uint16_t *vptr = pc_map_table_find_node( tptr, sptr );
  if ( !vptr ) {
    return PC_RETURN_SYMBOL_NOT_EXISTS;
  }
  pc_map_node_t *hn = &tptr->nds_[*vptr];
  for( p_uint16_t vidx = hn->sym_.pub_; vidx; ) {
    pc_map_node_t *pn = &tptr->nds_[vidx];
    vidx = pn->next_;
    if ( pc_pub_key_equal( pptr, &pn->pub_ ) ) {
      return PC_RETURN_PUBKEY_EXISTS;
    }
  }
  p_uint16_t pidx = pc_map_table_alloc_node( tptr );
  if ( pidx == 0 ) {
    return PC_RETURN_MAX_CAPACITY;
  }
  pc_map_node_t *pn = &tptr->nds_[pidx];
  pn->next_ = hn->sym_.pub_;
  pc_pub_key_assign( &pn->pub_, pptr );
  hn->sym_.pub_ = pidx;

  return PC_RETURN_SUCCESS;
}

/*
 * remove publisher key from symbol account
 */
p_uint64_t pc_map_table_del_publisher( pc_map_table_t *tptr,
                                       pc_symbol_t *sptr,
                                       pc_pub_key_t *pub )
{
  p_uint16_t *vptr = pc_map_table_find_node( tptr, sptr );
  if ( !vptr ) {
    return PC_RETURN_SYMBOL_NOT_EXISTS;
  }
  pc_map_node_t *nptr = &tptr->nds_[*vptr];
  vptr = pc_map_table_find_publisher( tptr, nptr, pub );
  if ( !vptr ) {
    return PC_RETURN_PUBKEY_NOT_EXISTS;
  }
  p_uint16_t pidx = *vptr;
  pc_map_node_t *pptr = &tptr->nds_[pidx];
  *vptr = pptr->next_;
  pc_map_table_dealloc_node( tptr, pidx );
  return PC_RETURN_SUCCESS;
}

/*
 * verify that account and publisher key is valid for some symbol
 */
p_uint64_t pc_map_table_verify( pc_map_table_t *tptr,
                                pc_symbol_t *sptr,
                                pc_pub_key_t *acc,
                                pc_pub_key_t *pub )

{
  p_uint16_t *vptr = pc_map_table_find_node( tptr, sptr );
  if ( !vptr ) {
    return PC_RETURN_SYMBOL_NOT_EXISTS;
  }
  pc_map_node_t *nptr = &tptr->nds_[*vptr];
  pc_map_node_t *an = &tptr->nds_[nptr->sym_.acc_];
  if ( !pc_pub_key_equal( &an->acc_, acc ) ) {
    return PC_RETURN_ACCOUNT_NOT_EXISTS;
  }
  vptr = pc_map_table_find_publisher( tptr, nptr, pub );
  if ( !vptr ) {
    return PC_RETURN_PUBKEY_NOT_EXISTS;
  }
  return PC_RETURN_SUCCESS;
}


#ifdef __cplusplus
}
#endif
