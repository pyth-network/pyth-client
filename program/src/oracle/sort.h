#pragma once

#ifdef __x86_64__
#include <stdint.h>
#else
#include <solana_sdk.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

void qsort_int64( int64_t* v, unsigned size );

#ifdef __cplusplus
}
#endif

