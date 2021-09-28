#pragma once

#ifdef __bpf__
#include <solana_sdk.h>
#else
#include <stdint.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

void qsort_int64( int64_t* v, unsigned size );

#ifdef __cplusplus
}
#endif

