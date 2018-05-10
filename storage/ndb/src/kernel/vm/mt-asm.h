/* Copyright (c) 2008, 2018, Oracle and/or its affiliates. All rights reserved.

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
 * Only memory barriers *must* be ported
 * if XCNG (x86-sematics) is provided, spinlocks will be enabled
 */
#ifndef NDB_MT_ASM_H
#define NDB_MT_ASM_H

/**
 * Remove comment on NDB_USE_SPINLOCK if it is desired to use spinlocks
 * instead of the normal mutex calls. This will not work when configuring
 * with realtime and is thus disabled by default, but can be activated for
 * special builds.
 */
//#define NDB_USE_SPINLOCK

#if defined(__GNUC__)
/********************
 * GCC
 *******************/
#if defined(__x86_64__) || defined (__i386__) /* 64 or 32 bit x86 */

#define NDB_HAVE_MB
#define NDB_HAVE_RMB
#define NDB_HAVE_WMB
#define NDB_HAVE_READ_BARRIER_DEPENDS
#define NDB_HAVE_XCNG
#define NDB_HAVE_CPU_PAUSE

/* Memory barriers, these definitions are for x64_64. */
#define mb()    asm volatile("mfence":::"memory")
/* According to Intel docs, it does not reorder loads. */
/* #define rmb() asm volatile("lfence":::"memory") */
#define rmb()   asm volatile("" ::: "memory")
#define wmb()   asm volatile("" ::: "memory")
#define read_barrier_depends()  do {} while(0)

static
inline
int
xcng(volatile unsigned * addr, int val)
{
  asm volatile ("xchg %0, %1;" : "+r" (val) , "+m" (*addr));
  return val;
}

static
inline
void
cpu_pause()
{
  asm volatile ("rep;nop");
}

#elif defined(__sparc__)

#define NDB_HAVE_MB
#define NDB_HAVE_RMB
#define NDB_HAVE_WMB
#define NDB_HAVE_READ_BARRIER_DEPENDS

#define mb()    asm volatile("membar #LoadLoad | #LoadStore | #StoreLoad | #StoreStore":::"memory")
#define rmb()   asm volatile("membar #LoadLoad" ::: "memory")
#define wmb()   asm volatile("membar #StoreStore" ::: "memory")
#define read_barrier_depends()  do {} while(0)

#ifdef HAVE_ATOMIC_H
#include <atomic.h>
#endif

#ifdef HAVE_ATOMIC_SWAP_32
static inline
int
xcng(volatile unsigned * addr, int val)
{
  asm volatile("membar #StoreLoad | #LoadLoad");
  int ret = atomic_swap_32(addr, val);
  asm volatile("membar #StoreLoad | #StoreStore");
  return ret;
}
#define cpu_pause()
#define NDB_HAVE_XCNG
#define NDB_HAVE_CPU_PAUSE
#else
#define cpu_pause()
/* link error if used incorrectly (i.e wo/ having NDB_HAVE_XCNG) */
extern  int xcng(volatile unsigned * addr, int val);
#endif

#elif defined(__powerpc__)
#define NDB_HAVE_MB
#define NDB_HAVE_RMB
#define NDB_HAVE_WMB
#define NDB_HAVE_READ_BARRIER_DEPENDS
#define NDB_HAVE_XCNG

#define mb() asm volatile("lwsync;" ::: "memory")
#define rmb() asm volatile("lwsync;" ::: "memory")
#define wmb() asm volatile("lwsync;" ::: "memory")
#define read_barrier_depends() do {} while(0)

static
inline
int
xcng(volatile unsigned * addr, int val)
{
  int prev;

  asm volatile ( "lwsync;\n"
		 "1: lwarx   %0,0,%2;"
		 "   stwcx.  %3,0,%2;"
		 "   bne-    1b;"
		 "isync;"
		 : "=&r" (prev), "+m" (*(volatile unsigned int *)addr)
		 : "r" (addr), "r" (val)
		 : "cc", "memory");

  return prev;
}

#else
#define NDB_NO_ASM "Unsupported architecture (gcc)"
#endif

#elif defined(__sun)
/********************
 * SUN STUDIO
 *******************/

/**
 * TODO check that asm ("") implies a compiler barrier
 *      i.e that it clobbers memory
 */
#if defined(__x86_64) || defined (__i386) /* 64 or 32 bit x86 */
#define NDB_HAVE_MB
#define NDB_HAVE_RMB
#define NDB_HAVE_WMB
#define NDB_HAVE_READ_BARRIER_DEPENDS

#define mb()    asm ("mfence")
/* According to Intel docs, it does not reorder loads. */
/* #define rmb() asm ("lfence") */
#define rmb()   asm ("")
#define wmb()   asm ("")
#define read_barrier_depends()  do {} while(0)

#elif defined(__sparc)
#define NDB_HAVE_MB
#define NDB_HAVE_RMB
#define NDB_HAVE_WMB
#define NDB_HAVE_READ_BARRIER_DEPENDS

#define mb() asm ("membar #LoadLoad | #LoadStore | #StoreLoad | #StoreStore")
#define rmb() asm ("membar #LoadLoad")
#define wmb() asm ("membar #StoreStore")
#define read_barrier_depends()  do {} while(0)
#else
#define NDB_NO_ASM "Unsupported architecture (sun studio)"
#error "Unsupported architecture (sun studio)"
#endif

#if defined(__x86_64) || defined (__i386) || defined(__sparc)
/**
 * we should probably use assembler for x86 aswell...
 *   but i'm not really sure how you do this in sun-studio :-(
 */
#ifdef HAVE_ATOMIC_H
#include <atomic.h>
#endif

#ifdef HAVE_ATOMIC_SWAP_32
#define NDB_HAVE_XCNG
#define NDB_HAVE_CPU_PAUSE
#if defined(__sparc)
static inline
int
xcng(volatile unsigned * addr, int val)
{
  asm ("membar #StoreLoad | #LoadLoad");
  int ret = atomic_swap_32(addr, val);
  asm ("membar #StoreLoad | #StoreStore");
  return ret;
}
#define cpu_pause()
#elif defined(__x86_64) || defined (__i386)
static inline
int
xcng(volatile unsigned * addr, int val)
{
  /**
   * TODO check that atomic_swap_32 on x86-64 with sun-studio implies
   *  proper barriers
   */
  int ret = atomic_swap_32(addr, val);
  return ret;
}
static
inline
void
cpu_pause()
{
  asm volatile ("rep;nop");
}
#endif
#else
#define cpu_pause()
/* link error if used incorrectly (i.e wo/ having NDB_HAVE_XCNG) */
extern  int xcng(volatile unsigned * addr, int val);
#endif
#endif
#elif defined (_MSC_VER)

#define NDB_HAVE_MB
#define NDB_HAVE_RMB
#define NDB_HAVE_WMB
#define NDB_HAVE_READ_BARRIER_DEPENDS

#include <windows.h>
#define mb()    MemoryBarrier()
#define read_barrier_depends()  do {} while(0)
#ifdef _DEBUG
#define rmb()   do {} while(0)
#define wmb()   do {} while(0)
#else
#include <intrin.h>
/********************
 * Microsoft
 *******************/
/* Using instrinsics available on all architectures */
#define rmb()   _ReadBarrier()
#define wmb()   _WriteBarrier()
#endif

#define NDB_HAVE_XCNG
#define NDB_HAVE_CPU_PAUSE

static inline
int
xcng(volatile unsigned * addr, int val)
{
  return InterlockedExchange((volatile LONG*)addr, val);
}

static
inline
void
cpu_pause()
{
  YieldProcessor();
}
#else
#define NDB_NO_ASM "Unsupported compiler"
#endif

#endif
