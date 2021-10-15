#pragma once

#ifdef __bpf__
#include <solana_sdk.h>
#else
#include <stdbool.h>
#include <stdint.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define PD_SCALE9     (1000000000L)
#define PD_EMA_MAX_DIFF 4145     // maximum slots before reset
#define PD_EMA_EXPO     (-9)     // exponent of temporary storage
#define PD_EMA_DECAY   (-117065) // 1e9*-log(2)/5921
#define PC_FACTOR_SIZE       18

#define EXP_BITS 5
#define EXP_MASK ( ( 1L << EXP_BITS ) - 1 )

#define pd_new( n,v,e ) {(n)->v_=v;(n)->e_=e;}
#define pd_set( r,n ) (r)[0] = (n)[0]
#define pd_new_scale(n,v,e) {(n)->v_=v;(n)->e_=e;pd_scale(n);}

// decimal number format
typedef struct pd
{
  int32_t  e_;
  int64_t  v_;
} pd_t;

void pd_scale( pd_t * );
bool pd_store( int64_t *, const pd_t * );
void pd_load( pd_t *, int64_t );
void pd_adjust( pd_t *, int e, const int64_t * );

void pd_mul( pd_t *, const pd_t *, const pd_t * );
void pd_div( pd_t *, pd_t *, pd_t * );
void pd_add( pd_t *, const pd_t *, const pd_t *, const int64_t * );
void pd_sub( pd_t *, const pd_t *, const pd_t *, const int64_t * );
void pd_sqrt( pd_t *, pd_t *, const int64_t * );

int pd_lt( const pd_t *, const pd_t *, const int64_t * );
int pd_gt( const pd_t *, const pd_t *, const int64_t * );

#ifdef __cplusplus
}
#endif

