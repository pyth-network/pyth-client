#ifndef _pyth_oracle_model_model_h_
#define _pyth_oracle_model_model_h_

#include "../util/compat_stdint.h"
#include <stdalign.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * This function computes the p25, p50 and p75 percentiles of the given quotes and
 * writes them to the given pointers. It also returns the sorted quotes array. Being
 * sorted is not necessary for this model to work, and is only relied upon by the
 * tests to verify the correctness of the model with more confidence.
 *
 * The quote array might get modified by this function.
 */
int64_t *
price_model_core( uint64_t  cnt,       /* Number of elements in quote */
                  int64_t * quote,     /* Assumes quote[i] for i in [0,cnt) is the i-th quote on input */
                  int64_t * _p25,      /* Assumes *_p25 is safe to write to the p25 model output */
                  int64_t * _p50,      /* Assumes *_p50 " */
                  int64_t * _p75);     /* Assumes *_p75 " */

#ifdef __cplusplus
}
#endif

#endif /* _pyth_oracle_model_model_h_ */
