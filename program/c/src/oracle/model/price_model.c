#include "price_model.h"
#include "../util/avg.h" /* For avg_2_int64 */

/*
 * In-place Heapsort implementation optimized for minimal compute unit usage in BPF.
 *
 * Initially it creates a max heap in linear time and then to get ascending
 * order it swaps the root with the last element and then sifts down the root.
 *
 * There are a lot of (j-1) or (j+1) math in the code which can be optimized by
 * thinking of a as 1-based array. Fortunately, BPF compiler optimizes that for us.
 */
void heapsort(int64_t * a, uint64_t n) {
  if (n <= 1) return;

  /*
   * This is a bottom-up heapify which is linear in time.
   */
  for (uint64_t i = n / 2 - 1;; --i) {
    int64_t root = a[i];
    uint64_t j = i * 2 + 1;
    while (j < n) {
      if (j + 1 < n && a[j] < a[j + 1]) ++j;
      if (root >= a[j]) break;
      a[(j - 1) / 2] = a[j];
      j = j * 2 + 1;
    }
    a[(j - 1) / 2] = root;

    if (i == 0) break;
  }

  for (uint64_t i = n - 1; i > 0; --i) {
    int64_t tmp = a[0];
    a[0] = a[i];
    a[i] = tmp;

    int64_t root = a[0];
    uint64_t j = 1;
    while (j < i) {
      if (j + 1 < i && a[j] < a[j + 1]) ++j;
      if (root >= a[j]) break;
      a[(j - 1) / 2] = a[j];
      j = j * 2 + 1;
    }
    a[(j - 1) / 2] = root;
  }
}

/*
 * Find the 25, 50, and 75 percentiles of the given quotes using heapsort.
 *
 * This implementation optimizes the price_model_core function for minimal compute unit usage in BPF.
 *
 * In Solana, each BPF instruction costs 1 unit of compute and is much different than a native code
 * execution time. Here are some of the differences:
 * 1. There is no cache, so memory access is much more expensive.
 * 2. The instruction set is very minimal, and there are only 10 registers available.
 * 3. The BPF compiler is not very good at optimizing the code.
 * 4. The stack size is limited and having extra stack frame has high overhead.
 *
 * This implementation is chosen among other implementations such as merge-sort, quick-sort, and quick-select
 * because it is very fast, has small number of instructions, and has a very small memory footprint by being
 * in-place and is non-recursive and has a nlogn worst-case time complexity.
 */
int64_t *
price_model_core( uint64_t  cnt,
                  int64_t * quote,
                  int64_t * _p25,
                  int64_t * _p50,
                  int64_t * _p75) {
  heapsort(quote, cnt);

  /* Extract the p25 */
  uint64_t p25_idx = cnt >> 2;
  *_p25 = quote[p25_idx];

  /* Extract the p50 */
  if( (cnt & (uint64_t)1) ) { /* Odd number of quotes */
    uint64_t p50_idx = cnt >> 1; /* ==ceil((cnt-1)/2) */
    *_p50 = quote[p50_idx];
  } else { /* Even number of quotes (at least 2) */
    uint64_t p50_idx_right = cnt >> 1;                    /* == ceil((cnt-1)/2)> 0 */
    uint64_t p50_idx_left  = p50_idx_right - (uint64_t)1; /* ==floor((cnt-1)/2)>=0 (no overflow/underflow) */

    int64_t vl = quote[p50_idx_left];
    int64_t vr = quote[p50_idx_right];

    /* Compute the average of vl and vr (with floor / round toward
       negative infinity rounding and without possibility of
       intermediate overflow). */
    *_p50 = avg_2_int64( vl, vr );
  }

  /* Extract the p75 (this is the mirror image of the p25 case) */

  uint64_t p75_idx = cnt - ((uint64_t)1) - p25_idx;
  *_p75 = quote[p75_idx];

  return quote;
}
