#ifndef ATOMIC_SOLARIS_INCLUDED
#define ATOMIC_SOLARIS_INCLUDED

/* Copyright (c) 2008, 2014, Oracle and/or its affiliates. All rights reserved.

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

#include <atomic.h>

#if defined(__GNUC__)
#define atomic_typeof(T,V)      __typeof__(V)
#else
#define atomic_typeof(T,V)      T
#endif

static inline int my_atomic_cas32(int32 volatile *a, int32 *cmp, int32 set)
{
  int ret;
  atomic_typeof(uint32_t, *cmp) sav;
  sav= atomic_cas_32((volatile uint32_t *)a, (uint32_t)*cmp, (uint32_t)set);
  ret= (sav == *cmp);
  if (!ret)
    *cmp= sav;
  return ret;
}

static inline int my_atomic_cas64(int64 volatile *a, int64 *cmp, int64 set)
{
  int ret;
  atomic_typeof(uint64_t, *cmp) sav;
  sav= atomic_cas_64((volatile uint64_t *)a, (uint64_t)*cmp, (uint64_t)set);
  ret= (sav == *cmp);
  if (!ret)
    *cmp= sav;
  return ret;
}

static inline int my_atomic_casptr(void * volatile *a, void **cmp, void *set)
{
  int ret;
  atomic_typeof(void *, *cmp) sav;
  sav= atomic_cas_ptr((volatile void **)a, (void *)*cmp, (void *)set);
  ret= (sav == *cmp);
  if (!ret)
    *cmp= sav;
  return ret;
}

static inline int32 my_atomic_add32(int32 volatile *a, int32 v)
{
  int32 nv= atomic_add_32_nv((volatile uint32_t *)a, v);
  return nv - v;
}

static inline int64 my_atomic_add64(int64 volatile *a, int64 v)
{
  int64 nv= atomic_add_64_nv((volatile uint64_t *)a, v);
  return nv - v;
}

static inline int32 my_atomic_fas32(int32 volatile *a, int32 v)
{
  return atomic_swap_32((volatile uint32_t *)a, (uint32_t)v);
}

static inline int64 my_atomic_fas64(int64 volatile *a, int64 v)
{
  return atomic_swap_64((volatile uint64_t *)a, (uint64_t)v);
}

static inline void * my_atomic_fasptr(void * volatile *a, void * v)
{
  return atomic_swap_ptr(a, v);
}

static inline int32 my_atomic_load32(int32 volatile *a)
{
  return atomic_or_32_nv((volatile uint32_t *)a, 0);
}

static inline int64 my_atomic_load64(int64 volatile *a)
{
  return atomic_or_64_nv((volatile uint64_t *)a, 0);
}

static inline void* my_atomic_loadptr(void * volatile *a)
{
  return atomic_add_ptr_nv(a, 0);
}

static inline void my_atomic_store32(int32 volatile *a, int32 v)
{
  (void) atomic_swap_32((volatile uint32_t *)a, (uint32_t)v);
}

static inline void my_atomic_store64(int64 volatile *a, int64 v)
{
  (void) atomic_swap_64((volatile uint64_t *)a, (uint64_t)v);
}

static inline void my_atomic_storeptr(void * volatile *a, void *v)
{
  (void) atomic_swap_ptr((volatile void **)a, (void *)v);
}

#endif /* ATOMIC_SOLARIS_INCLUDED */
