#ifndef MY_ATOMIC_INCLUDED
#define MY_ATOMIC_INCLUDED

/* Copyright (c) 2006, 2017, Oracle and/or its affiliates. All rights reserved.

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

/**
  @file include/my_atomic.h
*/

#if defined(_MSC_VER)

#include <windows.h>

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

#else  // !defined(_MSC_VER)

#define LF_BACKOFF (1)

#endif

#endif /* MY_ATOMIC_INCLUDED */
