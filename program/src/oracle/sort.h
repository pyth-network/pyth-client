#pragma once

#if defined( __bpf__ ) || defined( SOL_TEST )
#include <solana_sdk.h>
#else
#include <stdint.h> // NOLINT(modernize-deprecated-headers)
#endif

#ifdef __cplusplus
extern "C" {
#endif

void qsort_int64( int64_t* v, unsigned size );

#ifdef __cplusplus
}
#endif

