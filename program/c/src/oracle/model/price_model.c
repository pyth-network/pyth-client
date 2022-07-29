#include "price_model.h"
#include "../util/avg.h" /* For avg_2_int64 */

#define SORT_NAME  int64_sort_ascending
#define SORT_KEY_T int64_t
#include "../sort/tmpl/sort_stable.c"

int64_t *
price_model_core( uint64_t  cnt,
                  int64_t * quote,
                  int64_t * _p25,
                  int64_t * _p50,
                  int64_t * _p75,
                  void    * scratch ) {

  /* Sort the quotes.  The sorting implementation used here is a highly
     optimized mergesort (merge with an unrolled insertion sorting
     network small n base cases).  The best case is ~0.5 n lg n compares
     and the average and worst cases are ~n lg n compares.

     While not completely data oblivious, this has quite low variance in
     operation count practically and this is _better_ than quicksort's
     average case and quicksort's worst case is a computational
     denial-of-service and timing attack vulnerable O(n^2).  Unlike
     quicksort, this is also stable (but this stability does not
     currently matter ... it might be a factor in future models).

     A data oblivious sorting network approach might be viable here with
     and would have a completely deterministic operations count.  It
     currently isn't used as the best known practical approaches for
     general n have a worse algorithmic cost (O( n (lg n)^2 )) and,
     while the application probably doesn't need perfect obliviousness,
     mergesort is still moderately oblivious and the application can
     benefit from mergesort's lower operations cost.  (The main drawback
     of mergesort over quicksort is that it isn't in place, but memory
     footprint isn't an issue here.)

     Given the operations cost model (e.g. cache friendliness is not
     incorporated), a radix sort might be viable here (O(n) in best /
     average / worst).  It currently isn't used as we expect invocations
     with small-ish n to be common and radix sort would be have large
     coefficients on the O(n) and additional fixed overheads that would
     make it more expensive than mergesort in this regime.

     Note: price_model_cnt_valid( cnt ) implies
     int64_sort_ascending_cnt_valid( cnt ) currently.

     Note: consider filtering out "NaN" quotes (i.e. INT64_MIN)? */

  int64_t * sort_quote = int64_sort_ascending_stable( quote, cnt, scratch );

  /* Extract the p25

     There are many variants with subtle tradeoffs here.  One option is
     to interpolate when the ideal p25 is bracketed by two samples (akin
     to the p50 interpolation above when the number of quotes is even).
     That is, for p25, interpolate between quotes floor((cnt-2)/4) and
     ceil((cnt-2)/4) with the weights determined by cnt mod 4.  The
     current preference is to not do that as it is slightly more
     complex, doesn't exactly always minimize the current loss function
     and is more exposed to the confidence intervals getting skewed by
     bum quotes with the number of quotes is small.

     Another option is to use the inside quote of the above pair.  That
     is, for p25, use quote ceil((cnt-2)/4) == floor((cnt+1)/4) ==
     (cnt+1)>>2.  The current preference is not to do this as, though
     this has stronger bum quote robustness, it results in p25==p50==p75
     when cnt==3.  (In this case, the above wants to do an interpolation
     between quotes 0 and 1 to for the p25 and between quotes 1 and 2
     for the p75.  But limiting to just the inside quote results in
     p25/p50/p75 all using the median quote.)

     A tweak to this option, for p25, is to use floor(cnt/4) == cnt>>2.
     This is simple, has the same asymptotic behavior for large cnt, has
     good behavior in the cnt==3 case and practically as good bum quote
     rejection in the moderate cnt case. */

  uint64_t p25_idx = cnt >> 2;

  *_p25 = sort_quote[p25_idx];

  /* Extract the p50 */

  if( (cnt & (uint64_t)1) ) { /* Odd number of quotes */

    uint64_t p50_idx = cnt >> 1; /* ==ceil((cnt-1)/2) */

    *_p50 = sort_quote[p50_idx];

  } else { /* Even number of quotes (at least 2) */

    uint64_t p50_idx_right = cnt >> 1;                    /* == ceil((cnt-1)/2)> 0 */
    uint64_t p50_idx_left  = p50_idx_right - (uint64_t)1; /* ==floor((cnt-1)/2)>=0 (no overflow/underflow) */

    int64_t vl = sort_quote[p50_idx_left ];
    int64_t vr = sort_quote[p50_idx_right];

    /* Compute the average of vl and vr (with floor / round toward
       negative infinity rounding and without possibility of
       intermediate overflow). */

    *_p50 = avg_2_int64( vl, vr );
  }

  /* Extract the p75 (this is the mirror image of the p25 case) */

  uint64_t p75_idx = cnt - ((uint64_t)1) - p25_idx;

  *_p75 = sort_quote[p75_idx];

  return sort_quote;
}
