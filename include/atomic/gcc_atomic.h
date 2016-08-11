#ifndef GCC_ATOMIC_INCLUDED
#define GCC_ATOMIC_INCLUDED

/* Copyright (c) 2016, Oracle and/or its affiliates. All rights reserved.

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

/* New GCC __atomic builtins introduced in GCC 4.7 */

static inline int my_atomic_cas32(int32 volatile *a, int32 *cmp, int32 set)
{
  return __atomic_compare_exchange_n(a, cmp, set, 0,
                                     __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
}

static inline int my_atomic_cas64(int64 volatile *a, int64 *cmp, int64 set)
{
  return __atomic_compare_exchange_n(a, cmp, set, 0,
                                     __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
}

static inline int my_atomic_casptr(void * volatile *a, void **cmp, void *set)
{
  return __atomic_compare_exchange_n(a, cmp, set, 0,
                                     __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
}

static inline int32 my_atomic_add32(int32 volatile *a, int32 v)
{
  return __atomic_fetch_add(a, v, __ATOMIC_SEQ_CST);
}

static inline int64 my_atomic_add64(int64 volatile *a, int64 v)
{
  return __atomic_fetch_add(a, v, __ATOMIC_SEQ_CST);
}

static inline int32 my_atomic_fas32(int32 volatile *a, int32 v)
{
  return __atomic_exchange_n(a, v, __ATOMIC_SEQ_CST);
}

static inline int64 my_atomic_fas64(int64 volatile *a, int64 v)
{
  return __atomic_exchange_n(a, v, __ATOMIC_SEQ_CST);
}

static inline void * my_atomic_fasptr(void * volatile *a, void * v)
{
  return __atomic_exchange_n(a, v, __ATOMIC_SEQ_CST);
}

static inline int32 my_atomic_load32(int32 volatile *a)
{
  return __atomic_load_n(a, __ATOMIC_SEQ_CST);
}

static inline int64 my_atomic_load64(int64 volatile *a)
{
  return __atomic_load_n(a, __ATOMIC_SEQ_CST);
}

static inline void* my_atomic_loadptr(void * volatile *a)
{
  return __atomic_load_n(a, __ATOMIC_SEQ_CST);
}

static inline void my_atomic_store32(int32 volatile *a, int32 v)
{
  __atomic_store_n(a, v, __ATOMIC_SEQ_CST);
}

static inline void my_atomic_store64(int64 volatile *a, int64 v)
{
  __atomic_store_n(a, v, __ATOMIC_SEQ_CST);
}

static inline void my_atomic_storeptr(void * volatile *a, void *v)
{
  __atomic_store_n(a, v, __ATOMIC_SEQ_CST);
}

#endif /* GCC_ATOMIC_INCLUDED */
