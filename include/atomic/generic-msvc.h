/* Copyright (C) 2006-2008 MySQL AB, 2008-2009 Sun Microsystems, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#ifndef _atomic_h_cleanup_
#define _atomic_h_cleanup_ "atomic/generic-msvc.h"

/*
  We don't implement anything specific for MY_ATOMIC_MODE_DUMMY, always use
  intrinsics.
  8 and 16-bit atomics are not implemented, but it can be done if necessary.
*/
#undef MY_ATOMIC_HAS_8_16

#include <windows.h>
/*
  x86 compilers (both VS2003 or VS2005) never use instrinsics, but generate 
  function calls to kernel32 instead, even in the optimized build. 
  We force intrinsics as described in MSDN documentation for 
  _InterlockedCompareExchange.
*/
#ifdef _M_IX86

#if (_MSC_VER >= 1500)
#include <intrin.h>
#else
C_MODE_START
/*Visual Studio 2003 and earlier do not have prototypes for atomic intrinsics*/
LONG _InterlockedCompareExchange (LONG volatile *Target, LONG Value, LONG Comp);
LONGLONG _InterlockedCompareExchange64 (LONGLONG volatile *Target,
                                        LONGLONG Value, LONGLONG Comp);
C_MODE_END

#pragma intrinsic(_InterlockedCompareExchange)
#pragma intrinsic(_InterlockedCompareExchange64)
#endif

#define InterlockedCompareExchange _InterlockedCompareExchange
#define InterlockedCompareExchange64 _InterlockedCompareExchange64
/*
 No need to do something special for InterlockedCompareExchangePointer
 as it is a #define to InterlockedCompareExchange. The same applies to
 InterlockedExchangePointer. 
*/
#endif /*_M_IX86*/

#define MY_ATOMIC_MODE "msvc-intrinsics"
/* Implement using CAS on WIN32 */
#define IL_COMP_EXCHG32(X,Y,Z)  \
  InterlockedCompareExchange((volatile LONG *)(X),(Y),(Z))
#define IL_COMP_EXCHG64(X,Y,Z)  \
  InterlockedCompareExchange64((volatile LONGLONG *)(X), \
                               (LONGLONG)(Y),(LONGLONG)(Z))
#define IL_COMP_EXCHGptr        InterlockedCompareExchangePointer

#define make_atomic_cas_body(S)                                 \
  int ## S initial_cmp= *cmp;                                   \
  int ## S initial_a= IL_COMP_EXCHG ## S (a, set, initial_cmp); \
  if (!(ret= (initial_a == initial_cmp))) *cmp= initial_a;

#ifndef _M_IX86
/* Use full set of optimised functions on WIN64 */
#define IL_EXCHG_ADD32(X,Y)     \
  InterlockedExchangeAdd((volatile LONG *)(X),(Y))
#define IL_EXCHG_ADD64(X,Y)     \
  InterlockedExchangeAdd64((volatile LONGLONG *)(X),(LONGLONG)(Y))
#define IL_EXCHG32(X,Y)         \
  InterlockedExchange((volatile LONG *)(X),(Y))
#define IL_EXCHG64(X,Y)         \
  InterlockedExchange64((volatile LONGLONG *)(X),(LONGLONG)(Y))
#define IL_EXCHGptr             InterlockedExchangePointer

#define make_atomic_add_body(S) \
  v= IL_EXCHG_ADD ## S (a, v)
#define make_atomic_swap_body(S) \
  v= IL_EXCHG ## S (a, v)
#define make_atomic_load_body(S)       \
  ret= 0; /* avoid compiler warning */ \
  ret= IL_COMP_EXCHG ## S (a, ret, ret);
#endif
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
#ifndef YIELD_LOOPS
#define YIELD_LOOPS 200
#endif

static __inline int my_yield_processor()
{
  int i;
  for(i=0; i<YIELD_LOOPS; i++)
  {
#if (_MSC_VER <= 1310)
    /* On older compilers YieldProcessor is not available, use inline assembly*/
    __asm { rep nop }
#else
    YieldProcessor();
#endif
  }
  return 1;
}

#define LF_BACKOFF my_yield_processor()
#else /* cleanup */

#undef IL_EXCHG_ADD32
#undef IL_EXCHG_ADD64
#undef IL_COMP_EXCHG32
#undef IL_COMP_EXCHG64
#undef IL_COMP_EXCHGptr
#undef IL_EXCHG32
#undef IL_EXCHG64
#undef IL_EXCHGptr

#endif
