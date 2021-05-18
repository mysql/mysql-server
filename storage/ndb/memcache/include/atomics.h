/*
 Copyright (c) 2011, 2016, Oracle and/or its affiliates. All rights reserved.
 
 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License, version 2.0,
 as published by the Free Software Foundation.

 This program is also distributed with certain software (including
 but not limited to OpenSSL) that is licensed under separate terms,
 as designated in a particular file or component or in included license
 documentation.  The authors of MySQL hereby grant you an additional
 permission to link the program and your derivative works with the
 separately licensed software that they have included with MySQL.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License, version 2.0, for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
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
#elif defined HAVE_GCC_ATOMIC_BUILTINS || defined HAVE_GCC_ATOMICS_WITH_ARCH_FLAG || defined HAVE_GCC_SYNC_BUILTINS
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
typedef volatile Int32 atomic_int32_t;
#else
typedef volatile Uint32 atomic_int32_t;
#endif


/* With Darwin atomics and gcc intrinsic atomics, some calls are macros.
*/

DECLARE_FUNCTIONS_WITH_C_LINKAGE

#ifdef USE_DARWIN_ATOMICS
#define atomic_cmp_swap_int(loc, old, new) OSAtomicCompareAndSwap32Barrier(old, new, loc)
void atomic_set_ptr(void * volatile * target, void *);
#define atomic_barrier() OSMemoryBarrier()

#elif defined USE_GCC_ATOMICS
#define atomic_cmp_swap_int(loc, old, new) \
  __sync_bool_compare_and_swap(loc, (Uint32) old, (Uint32) new)
void atomic_set_ptr(void * volatile * target, void *);
#define atomic_barrier() __sync_synchronize()

#elif defined USE_SOLARIS_ATOMICS
int atomic_cmp_swap_int(atomic_int32_t *loc, int oldvalue, int newvalue);
void atomic_set_ptr(void * volatile * target, void *);
#define atomic_barrier()

#else
#error No atomic functions selected.
#endif

END_FUNCTIONS_WITH_C_LINKAGE

#endif
