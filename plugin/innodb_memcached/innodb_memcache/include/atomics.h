#ifndef NDBMEMCACHE_ATOMIC_H
#define NDBMEMCACHE_ATOMIC_H

#include "config.h"
#include "dbmemcache_global.h"


/* This section of code determines which library to use; it is possible that
   more than one of them is available, so they are tested in order of 
   preference.
*/
#if defined HAVE_SOLARIS_ATOMICS
#define USE_SOLARIS_ATOMICS
#elif defined HAVE_DARWIN_ATOMICS
#define USE_DARWIN_ATOMICS 
#elif defined HAVE_GCC_ATOMIC_BUILTINS
#define USE_GCC_ATOMICS
#else 
#error No atomic functions available.
#endif 


/* The next section selects needed headers for the chosen functions.
*/
#if defined USE_DARWIN_ATOMICS
#include <libkern/OSAtomic.h>

#elif defined USE_SOLARIS_ATOMICS
#include <atomic.h>

#elif defined USE_GCC_ATOMICS
/* No header file required for GCC intrinsic atomic functions */
#endif 


/* Some native atomic functions want signed ints, and others want unsigned ints.
   In all cases the values are declared "volatile". 
   We define a type here. 
*/   
#ifdef USE_DARWIN_ATOMICS
typedef volatile int32_t ndbmc_atomic32_t;
#else
typedef volatile uint32_t ndbmc_atomic32_t; 
#endif


/* With Darwin atomics, atomic_cmp_swap_int and atomic_cmp_swap_ptr are 
   macros.  On other platforms we declare them as functions.
*/

#ifdef USE_DARWIN_ATOMICS
#define atomic_cmp_swap_int(loc, old, new) OSAtomicCompareAndSwap32Barrier(old, new, loc)
#define atomic_cmp_swap_ptr(loc, old, new) OSAtomicCompareAndSwapPtrBarrier(old, new, loc)

#else

DECLARE_FUNCTIONS_WITH_C_LINKAGE

bool atomic_cmp_swap_int(ndbmc_atomic32_t *loc, int oldvalue, int newvalue);
bool atomic_cmp_swap_ptr(volatile void **loc, void *oldvalue, void *newvalue);

END_FUNCTIONS_WITH_C_LINKAGE
#endif

#endif
