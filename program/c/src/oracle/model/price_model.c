#include "price_model.h"
#include "../util/avg.h" /* For avg_2_int64 */

/*
 * Quick select implementation highly optimized for reducing the number of compute units in BPF.
 *
 * On Floyd and Rivest's SELECT algorithm" by Krzysztof C. Kiwiel article
 * mentions that the average case of this algorithm is is 2.5n + o(n) comparisons.
 * The article itself presents a different algorithm (Floyd and Rivest) which
 * performs faster in larger arrays but has a higher overhead. Other determinstic
 * algorithms like median of medians have a higher overhead (or comibnations of
 * them and quick select) and are not suitable for BPF.
 */
int64_t quick_select( int64_t * a, uint64_t n, uint64_t k ) {
    while (1) {
        if (n == 1) return a[0];

        int64_t pivot;

        /*
         * Select median of 3 as pivot which massively reduces the number of
         * comparisons in the average case and also helps in reducing the likelihood
         * of worst case time complexity.
         */
        {
            if (a[0] < a[n / 2]) {
                if (a[n / 2] < a[n - 1]) pivot = a[n / 2];
                else if (a[0] < a[n - 1]) pivot = a[n - 1];
                else pivot = a[0];
            } else {
                if (a[n / 2] > a[n - 1]) pivot = a[n / 2];
                else if (a[0] > a[n - 1]) pivot = a[n - 1];
                else pivot = a[0];
            }
        }

        /*
         * Partition the array around the pivot. This partitioning is done in a
         * way that if the array has a lot of duplicate pivots, it doesn't result in
         * quadratic time complexity and evenly distributes the pivots.
         *
         * When the loop is done, the array looks like any of the following:
         * 1. ....ji....
         * In this case the elements a[x] for x <= j are <= pivot and a[x] for x >= i are >= pivot.
         *
         * 2. ....j.i...
         * In this case same conditions as above apply but a[j+1] == pivot because the last step
         * resulting in j and i in this position is i==j and swapping which means that that element
         * is neither < pivot nor > pivot.
         *
         * In the case of multiple pivot elements, the elements will be evenly distributed because
         * i nor j won't move if the element is equal to the pivot and if both be equal to the pivot
         * they will swap (which is ineffective) and move forward which results in one pivot to the
         * left and one to the right.
         */
        uint64_t i = 0, j = n - 1;
        while (i <= j) {
            if (a[i] < pivot) {
                i++;
                continue;
            }
            if (a[j] > pivot) {
                j--;
                continue;
            }

            int64_t tmp = a[i];
            a[i] = a[j];
            a[j] = tmp;
            i++;
            j--;
        }

        if (k < j + 1) n = j + 1;
        else if (j + 2 == i && k == j + 1) return pivot;
        else {
            a += j + 1;
            n -= j + 1;
            k -= j + 1;
        }
    }
}

void
price_model_core( uint64_t  cnt,
                  int64_t * quote,
                  int64_t * _p25,
                  int64_t * _p50,
                  int64_t * _p75) {

  /* Extract the p50 */

  if( (cnt & (uint64_t)1) ) { /* Odd number of quotes */

    uint64_t p50_idx = cnt >> 1; /* ==ceil((cnt-1)/2) */

    *_p50 = quick_select(quote, cnt, p50_idx);

  } else { /* Even number of quotes (at least 2) */

    uint64_t p50_idx_right = cnt >> 1;                    /* == ceil((cnt-1)/2)> 0 */
    uint64_t p50_idx_left  = p50_idx_right - (uint64_t)1; /* ==floor((cnt-1)/2)>=0 (no overflow/underflow) */

    int64_t vl = quick_select(quote, cnt, p50_idx_left);
    int64_t vr = quick_select(quote, cnt, p50_idx_right);

    /* Compute the average of vl and vr (with floor / round toward
       negative infinity rounding and without possibility of
       intermediate overflow). */

    *_p50 = avg_2_int64( vl, vr );
  }

  /* Extract the p25 */

  uint64_t p25_idx = cnt >> 2;

  *_p25 = quick_select(quote, cnt, p25_idx);

  /* Extract the p75 (this is the mirror image of the p25 case) */

  uint64_t p75_idx = cnt - ((uint64_t)1) - p25_idx;

  *_p75 = quick_select(quote, cnt, p75_idx);
}
