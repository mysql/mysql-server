/*
 Copyright (c) 2011, Oracle and/or its affiliates. All rights
 reserved.
 
 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; version 2 of
 the License.
 
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 GNU General Public License for more details.
 
 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 02110-1301  USA
 */
#ifndef NDBMEMCACHE_ATOMIC_H
#define NDBMEMCACHE_ATOMIC_H

#include "ndbmemcache_config.h"
#include "ndbmemcache_global.h"

/* This section of code determines which library to use; it is possible that
   more than one of them is available, so they are tested in order of 
   preference.
*/
#if defined HAVE_SOLARIS_ATOMICS
#define USE_SOLARIS_ATOMICS
#elif defined HAVE_DARWIN_ATOMICS
#define USE_DARWIN_ATOMICS 
#elif defined HAVE_GCC_ATOMIC_BUILTINS || defined HAVE_GCC_ATOMICS_WITH_ARCH_FLAG
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
typedef volatile Int32 ndbmc_atomic32_t;
#else
typedef volatile Uint32 ndbmc_atomic32_t; 
#endif


/* With Darwin atomics and gcc intrinsic atomics, these calls are macros.
   On Solaris they are functions.
*/

#ifdef USE_DARWIN_ATOMICS
#define atomic_add_int(loc, amount) OSAtomicAdd32Barrier(amount, loc)
#define atomic_cmp_swap_int(loc, old, new) OSAtomicCompareAndSwap32Barrier(old, new, loc)
#define atomic_cmp_swap_ptr(loc, old, new) OSAtomicCompareAndSwapPtrBarrier(old, new, loc)

#elif defined USE_GCC_ATOMICS
#define atomic_cmp_swap_int(loc, old, new) \
  __sync_bool_compare_and_swap(loc, (Uint32) old, (Uint32) new)
#define atomic_cmp_swap_ptr(loc, old, new) __sync_bool_compare_and_swap(loc, old, new)
#define atomic_add_int(loc, amount) __sync_fetch_and_add(loc, amount)

#else

DECLARE_FUNCTIONS_WITH_C_LINKAGE

int atomic_cmp_swap_int(ndbmc_atomic32_t *loc, int oldvalue, int newvalue);
int atomic_cmp_swap_ptr(void * volatile *loc, void *oldvalue, void *newvalue);

END_FUNCTIONS_WITH_C_LINKAGE
#endif

#endif
