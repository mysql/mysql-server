/*
 Copyright (c) 2011, 2017, Oracle and/or its affiliates. All rights reserved.
 
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

#include "atomics.h"

#if defined USE_SOLARIS_ATOMICS 

int atomic_cmp_swap_int(atomic_int32_t *loc, int old, int newval) {
  int stored_old;
  
  membar_enter();
  stored_old = atomic_cas_32(loc, old, newval);
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
