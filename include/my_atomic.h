#ifndef MY_ATOMIC_INCLUDED
#define MY_ATOMIC_INCLUDED

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

/*
  This header defines five atomic operations:

  my_atomic_add#(&var, what)
    'Fetch and Add'
    add 'what' to *var, and return the old value of *var

  my_atomic_fas#(&var, what)
    'Fetch And Store'
    store 'what' in *var, and return the old value of *var

  my_atomic_cas#(&var, &old, new)
    An odd variation of 'Compare And Set/Swap'
    if *var is equal to *old, then store 'new' in *var, and return TRUE
    otherwise store *var in *old, and return FALSE
    Usually, &old should not be accessed if the operation is successful.

  my_atomic_load#(&var)
    return *var

  my_atomic_store#(&var, what)
    store 'what' in *var

  '#' is substituted by a size suffix - 32, 64, or ptr
  (e.g. my_atomic_add64, my_atomic_fas32, my_atomic_casptr).
*/

/*
  We choose implementation as follows:
  ------------------------------------
  On Windows using Visual C++ the native implementation should be
  preferrable. When using gcc we prefer the Solaris implementation
  before the gcc because of stability preference, we choose gcc
  builtins if available. If neither Visual C++ or gcc we still choose
  the Solaris implementation on Solaris (mainly for SunStudio compilers).
*/
#if defined(_MSC_VER)
#  include "atomic/generic-msvc.h"
#elif defined(HAVE_SOLARIS_ATOMIC)
#  include "atomic/solaris.h"
#elif defined(HAVE_GCC_ATOMIC_BUILTINS)
#  include "atomic/gcc_builtins.h"
#else
#  error Native atomics support not found!
#endif

/*
  the macro below defines (as an expression) the code that will be run in
  spin-loops. Intel manuals recommend to have PAUSE there.
*/
#ifdef HAVE_PAUSE_INSTRUCTION
   /*
      According to the gcc info page, asm volatile means that the instruction
      has important side-effects and must not be removed.  Also asm volatile may
      trigger a memory barrier (spilling all registers to memory).
   */
#  define MY_PAUSE() __asm__ __volatile__ ("pause")
# elif defined(HAVE_FAKE_PAUSE_INSTRUCTION)
#  define UT_PAUSE() __asm__ __volatile__ ("rep; nop")
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
