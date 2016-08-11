#ifndef GCC_SYNC_INCLUDED
#define GCC_SYNC_INCLUDED

/* Copyright (c) 2008, 2016, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/* Old GCC __sync builtins introduced in GCC 4.1 */

static inline int my_atomic_cas32(int32 volatile *a, int32 *cmp, int32 set)
{
  int32 cmp_val= *cmp;
  int32 sav= __sync_val_compare_and_swap(a, cmp_val, set);
  int ret= (sav == cmp_val);
  if (!ret)
    *cmp = sav;
  return ret;
}

static inline int my_atomic_cas64(int64 volatile *a, int64 *cmp, int64 set)
{
  int64 cmp_val= *cmp;
  int64 sav= __sync_val_compare_and_swap(a, cmp_val, set);
  int ret= (sav == cmp_val);
  if (!ret)
    *cmp = sav;
  return ret;
}

static inline int my_atomic_casptr(void * volatile *a, void **cmp, void *set)
{
  void *cmp_val= *cmp;
  void *sav= __sync_val_compare_and_swap(a, cmp_val, set);
  int ret= (sav == cmp_val);
  if (!ret)
    *cmp = sav;
  return ret;
}

static inline int32 my_atomic_add32(int32 volatile *a, int32 v)
{
  return __sync_fetch_and_add(a, v);
}

static inline int64 my_atomic_add64(int64 volatile *a, int64 v)
{
  return __sync_fetch_and_add(a, v);
}

static inline int32 my_atomic_fas32(int32 volatile *a, int32 v)
{
  return __sync_lock_test_and_set(a, v);
}

static inline int64 my_atomic_fas64(int64 volatile *a, int64 v)
{
  return __sync_lock_test_and_set(a, v);
}

static inline void * my_atomic_fasptr(void * volatile *a, void * v)
{
  return __sync_lock_test_and_set(a, v);
}

static inline int32 my_atomic_load32(int32 volatile *a)
{
  return __sync_fetch_and_or(a, 0);
}

static inline int64 my_atomic_load64(int64 volatile *a)
{
  return __sync_fetch_and_or(a, 0);
}

static inline void* my_atomic_loadptr(void * volatile *a)
{
  return __sync_fetch_and_or(a, 0);
}

static inline void my_atomic_store32(int32 volatile *a, int32 v)
{
  (void) __sync_lock_test_and_set(a, v);
}

static inline void my_atomic_store64(int64 volatile *a, int64 v)
{
  (void) __sync_lock_test_and_set(a, v);
}

static inline void my_atomic_storeptr(void * volatile *a, void *v)
{
  (void) __sync_lock_test_and_set(a, v);
}

#endif /* GCC_SYNC_INCLUDED */
