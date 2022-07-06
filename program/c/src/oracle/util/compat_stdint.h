#ifndef _pyth_oracle_util_compat_stdint_h_
#define _pyth_oracle_util_compat_stdint_h_

/* Include functionality from <stdint.h>.  Define
   PYTH_ORACLE_UTIL_COMPAT_STDINT_STYLE to indicate how to do this:
     0 - Use stdint.h directly
     1 - Use solana_sdk.h (solana uses its own definitions for stdint
         types and that can conflicts with stdint.h)
   Defaults to 0 or 1 depending on if __bpf__ is set. */


#ifndef PYTH_ORACLE_UTIL_COMPAT_STDINT_STYLE
#if !defined(__bpf__) && !defined(SOL_TEST)
#define PYTH_ORACLE_UTIL_COMPAT_STDINT_STYLE 0
#else
#define PYTH_ORACLE_UTIL_COMPAT_STDINT_STYLE 1
#endif
#endif


#if PYTH_ORACLE_UTIL_COMPAT_STDINT_STYLE==0
#include <stdint.h>
#elif PYTH_ORACLE_UTIL_COMPAT_STDINT_STYLE==1
#include <solana_sdk.h>
typedef uint64_t uintptr_t;
#else
#error "Unsupported PYTH_ORACLE_UTIL_COMPAT_STDINT_STYLE"
#endif

#endif /* _pyth_oracle_util_compat_stdint_h_ */
