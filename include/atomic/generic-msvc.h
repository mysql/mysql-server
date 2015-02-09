#ifndef ATOMIC_MSC_INCLUDED
#define ATOMIC_MSC_INCLUDED

/* Copyright (c) 2006, 2014, Oracle and/or its affiliates. All rights reserved.

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

#include <windows.h>

static inline int my_atomic_cas32(int32 volatile *a, int32 *cmp, int32 set)
{
  int32 initial_cmp= *cmp;
  int32 initial_a= InterlockedCompareExchange((volatile LONG*)a,
                                              set, initial_cmp);
  int ret= (initial_a == initial_cmp);
  if (!ret)
    *cmp= initial_a;
  return ret;
}

static inline int my_atomic_cas64(int64 volatile *a, int64 *cmp, int64 set)
{
  int64 initial_cmp= *cmp;
  int64 initial_a= InterlockedCompareExchange64((volatile LONGLONG*)a,
                                                (LONGLONG)set,
                                                (LONGLONG)initial_cmp);
  int ret= (initial_a == initial_cmp);
  if (!ret)
    *cmp= initial_a;
  return ret;
}

static inline int my_atomic_casptr(void * volatile *a, void **cmp, void *set)
{
  void *initial_cmp= *cmp;
  void *initial_a= InterlockedCompareExchangePointer(a, set, initial_cmp);
  int ret= (initial_a == initial_cmp);
  if (!ret)
    *cmp= initial_a;
  return ret;
}

static inline int32 my_atomic_add32(int32 volatile *a, int32 v)
{
  return (int32)InterlockedExchangeAdd((volatile LONG*)a, v);
}

static inline int64 my_atomic_add64(int64 volatile *a, int64 v)
{
  return (int64)InterlockedExchangeAdd64((volatile LONGLONG*)a, (LONGLONG)v);
}

static inline int32 my_atomic_load32(int32 volatile *a)
{
  return (int32)InterlockedCompareExchange((volatile LONG *)a, 0, 0);
}

static inline int64 my_atomic_load64(int64 volatile *a)
{
  return (int64)InterlockedCompareExchange64((volatile LONGLONG *)a, 0, 0);
}

static inline void* my_atomic_loadptr(void * volatile *a)
{
  return InterlockedCompareExchangePointer(a, 0, 0);
}

static inline int32 my_atomic_fas32(int32 volatile *a, int32 v)
{
  return (int32)InterlockedExchange((volatile LONG*)a, v);
}

static inline int64 my_atomic_fas64(int64 volatile *a, int64 v)
{
  return (int64)InterlockedExchange64((volatile LONGLONG*)a, v);
}

static inline void * my_atomic_fasptr(void * volatile *a, void * v)
{
  return InterlockedExchangePointer(a, v);
}

static inline void my_atomic_store32(int32 volatile *a, int32 v)
{
  (void)InterlockedExchange((volatile LONG*)a, v);
}

static inline void my_atomic_store64(int64 volatile *a, int64 v)
{
  (void)InterlockedExchange64((volatile LONGLONG*)a, v);
}

static inline void my_atomic_storeptr(void * volatile *a, void *v)
{
  (void)InterlockedExchangePointer(a, v);
}


/*
  my_yield_processor (equivalent of x86 PAUSE instruction) should be used
  to improve performance on hyperthreaded CPUs. Intel recommends to use it in
  spin loops also on non-HT machines to reduce power consumption (see e.g
  http://softwarecommunity.intel.com/articles/eng/2004.htm)

  Running benchmarks for spinlocks implemented with InterlockedCompareExchange
  and YieldProcessor shows that much better performance is achieved by calling
  YieldProcessor in a loop - that is, yielding longer. On Intel boxes setting
  loop count in the range 200-300 brought best results.
 */
#define YIELD_LOOPS 200

static inline int my_yield_processor()
{
  int i;
  for (i=0; i<YIELD_LOOPS; i++)
  {
    YieldProcessor();
  }
  return 1;
}

#define LF_BACKOFF my_yield_processor()

#endif /* ATOMIC_MSC_INCLUDED */
