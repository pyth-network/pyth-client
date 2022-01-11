#ifndef _pyth_oracle_util_compat_stdint_h_
#define _pyth_oracle_util_compat_stdint_h_

/* solana uses its own definitions for stdint types and that can cause
   problems with compilation */

#ifdef __bpf__
#include <solana_sdk.h>
typedef uint64_t uintptr_t;
#else
#include <stdint.h>
#endif

#endif /* _pyth_oracle_util_compat_stdint_h_ */
