#ifndef MY_ATOMIC_INCLUDED
#define MY_ATOMIC_INCLUDED

/* Copyright (c) 2006, 2017, Oracle and/or its affiliates. All rights reserved.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/**
  @file include/my_atomic.h
*/

#if defined(_MSC_VER)
#include <windows.h>
#endif

#include "my_compiler.h"

/*
  the macro below defines (as an expression) the code that
  will be run in spin-loops. Intel manuals recummend to have PAUSE there.
*/
#ifdef HAVE_PAUSE_INSTRUCTION
   /*
      According to the gcc info page, asm volatile means that the instruction
      has important side-effects and must not be removed.  Also asm volatile may
      trigger a memory barrier (spilling all registers to memory).
   */
#  define MY_PAUSE() __asm__ __volatile__ ("pause")
# elif defined(HAVE_FAKE_PAUSE_INSTRUCTION)
#  define MY_PAUSE() __asm__ __volatile__ ("rep; nop")
# elif defined(_MSC_VER)
   /*
      In the Win32 API, the x86 PAUSE instruction is executed by calling the
      YieldProcessor macro defined in WinNT.h. It is a CPU architecture-
      independent way by using YieldProcessor.
   */
#  define MY_PAUSE() YieldProcessor()
# else
#  define MY_PAUSE() ((void) 0)
#endif

/*
  POWER-specific macros to relax CPU threads to give more core resources to
  other threads executing in the core.
*/
#if defined(HAVE_HMT_PRIORITY_INSTRUCTION)
#  define MY_LOW_PRIORITY_CPU() __asm__ __volatile__ ("or 1,1,1")
#  define MY_RESUME_PRIORITY_CPU() __asm__ __volatile__ ("or 2,2,2")
#else
#  define MY_LOW_PRIORITY_CPU() ((void)0)
#  define MY_RESUME_PRIORITY_CPU() ((void)0)
#endif

/*
  my_yield_processor (equivalent of x86 PAUSE instruction) should be used to
  improve performance on hyperthreaded CPUs. Intel recommends to use it in spin
  loops also on non-HT machines to reduce power consumption (see e.g
  http://softwarecommunity.intel.com/articles/eng/2004.htm)

  Running benchmarks for spinlocks implemented with InterlockedCompareExchange
  and YieldProcessor shows that much better performance is achieved by calling
  YieldProcessor in a loop - that is, yielding longer. On Intel boxes setting
  loop count in the range 200-300 brought best results.
 */
#define MY_YIELD_LOOPS 200

static inline int my_yield_processor()
{
  int i;

  MY_LOW_PRIORITY_CPU();

  for (i= 0; i < MY_YIELD_LOOPS; i++)
  {
    MY_COMPILER_BARRIER();
    MY_PAUSE();
  }

  MY_RESUME_PRIORITY_CPU();

  return 1;
}

#endif /* MY_ATOMIC_INCLUDED */
