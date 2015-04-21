/*
 Copyright (c) 2011, 2015 Oracle and/or its affiliates. All rights
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

#include "atomics.h"

#if defined USE_SOLARIS_ATOMICS 

int atomic_cmp_swap_int(atomic_int32_t *loc, int old, int new) {
  int stored_old;
  
  membar_enter();
  stored_old = atomic_cas_32(loc, old, new);
  membar_exit();
  return (stored_old == old);  
}

void atomic_set_ptr(void * volatile * target, void *newval) {
  membar_enter();
  atomic_swap_ptr(target, newval);
  membar_exit();
}

#elif defined USE_GCC_ATOMICS

void atomic_set_ptr(void * volatile * target, void *newval) {
  int r;
  do {
    void * old = *target;
    r = __sync_bool_compare_and_swap(target, old, newval);
  } while(!r);
}

#elif defined USE_DARWIN_ATOMICS

void atomic_set_ptr(void * volatile * target, void *newval) {
  bool did_swap;
  do {
    did_swap = OSAtomicCompareAndSwapPtr(*target, newval, target);
  } while (! did_swap);
}

#endif
