#ifndef _pyth_oracle_model_model_h_
#define _pyth_oracle_model_model_h_

#include <stdint.h>
#include <stdalign.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Returns the minimum and maximum number of quotes the implementation
   can handle */

static inline uint64_t
price_model_quote_min( void ) {
  return (uint64_t)1;
}

static inline uint64_t
price_model_quote_max( void ) {
  return (UINT64_MAX-(uint64_t)alignof(int64_t)+(uint64_t)1) / (uint64_t)sizeof(int64_t);
}

/* price_model_cnt_valid returns non-zero if cnt is a valid value or
   zero if not. */

static inline int
price_model_cnt_valid( uint64_t cnt ) {
  return price_model_quote_min()<=cnt && cnt<=price_model_quote_max();
}

/* price_model_scratch_footprint returns the number of bytes of scratch
   space needed for an arbitrarily aligned scratch region required by
   price_model to handle price_model_quote_min() to cnt quotes
   inclusive. */

static inline uint64_t
price_model_scratch_footprint( uint64_t cnt ) { /* Assumes price_model_cnt_valid( cnt ) is true */
  /* cnt int64_t's plus worst case alignment padding, no overflow
     possible as cnt is valid at this point */
  return cnt*(uint64_t)sizeof(int64_t)+(uint64_t)alignof(int64_t)-(uint64_t)1;
}

/* price_model_core minimizes (to quote precision in a floor / round
   toward negative infinity sense) the loss model of the given quotes.
   Assumes valid inputs (e.g. cnt is at least 1 and not unreasonably
   large ... typically a multiple of 3 but this is not required,
   quote[i] for i in [0,cnt) are the quotes of interest on input, p25,
   p50, p75 point to where to write model outputs, scratch points to a
   suitable footprint srcatch region).

   Returns a pointer to the quotes sorted in ascending order.  As such,
   the min and max and any other rank statistic can be extracted easily
   on return.  This location will either be quote itself or to a
   location in scratch.  Use price_model below for a variant that always
   replaces quote with the sorted quotes (potentially has extra ops for
   copying).  Further, on return, *_p25, *_p50, *_p75 will hold the loss
   model minimizing values for the input quotes and the scratch region
   was clobbered.

   Scratch points to a memory region of arbitrary alignment with at
   least price_model_scratch_footprint( cnt ) bytes and it will be
   clobbered on output.  It is sufficient to use a normally aligned /
   normally allocated / normally declared array of cnt int64_t's.

   The cost of this function is a fast and low variance (but not
   completely data oblivious) O(cnt lg cnt) in the best / average /
   worst cases.  This function uses no heap / dynamic memory allocation.
   It is thread safe provided it passed non-conflicting quote, output
   and scratch arrays.  It has a bounded call depth ~lg cnt <= ~64 (this
   could reducd to O(1) by using a non-recursive sort/select
   implementation under the hood if desired). */

int64_t *                              /* Returns pointer to sorted quotes (either quote or ALIGN_UP(scratch,int64_t)) */
price_model_core( uint64_t  cnt,       /* Assumes price_model_cnt_valid( cnt ) is true */
                  int64_t * quote,     /* Assumes quote[i] for i in [0,cnt) is the i-th quote on input */
                  int64_t * _p25,      /* Assumes *_p25 is safe to write to the p25 model output */
                  int64_t * _p50,      /* Assumes *_p50 " */
                  int64_t * _p75,      /* Assumes *_p75 " */
                  void    * scratch ); /* Assumes a suitable scratch region */

/* Same as the above but always returns quote and quote always holds the
   sorted quotes on return. */

static inline int64_t *
price_model( uint64_t  cnt,
             int64_t * quote,
             int64_t * _p25,
             int64_t * _p50,
             int64_t * _p75,
             void    * scratch ) {
  int64_t * tmp = price_model_core( cnt, quote, _p25, _p50, _p75, scratch );
  if( tmp!=quote ) for( uint64_t idx=(uint64_t)0; idx<cnt; idx++ ) quote[ idx ] = tmp[ idx ];
  return quote;
}

#ifdef __cplusplus
}
#endif

#endif /* _pyth_oracle_model_model_h_ */
