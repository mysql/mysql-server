/* Copyright (c) 2008, 2023, Oracle and/or its affiliates.

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

#include <config.h>

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
#define NDB_HAVE_XCNG
#define NDB_HAVE_CPU_PAUSE

/* Memory barriers, these definitions are for x64_64. */
#define mb()    asm volatile("mfence":::"memory")
/* According to Intel docs, it does not reorder loads. */
/* #define rmb() asm volatile("lfence":::"memory") */
#define rmb()   asm volatile("" ::: "memory")
#define wmb()   asm volatile("" ::: "memory")

static
inline
int
xcng(volatile unsigned * addr, int val)
{
  asm volatile ("xchg %0, %1;" : "+r" (val) , "+m" (*addr));
  return val;
}

#if defined(HAVE_PAUSE_INSTRUCTION)
static
inline
void
cpu_pause()
{
  __asm__ __volatile__ ("pause");
}
#else
static
inline
void
cpu_pause()
{
  asm volatile ("rep;nop");
}
#endif

#elif defined(__sparc__)

#define NDB_HAVE_MB
#define NDB_HAVE_RMB
#define NDB_HAVE_WMB

#define mb()    asm volatile("membar #LoadLoad | #LoadStore | #StoreLoad | #StoreStore":::"memory")
#define rmb()   asm volatile("membar #LoadLoad" ::: "memory")
#define wmb()   asm volatile("membar #StoreStore" ::: "memory")

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
#define NDB_HAVE_XCNG
#else
/* link error if used incorrectly (i.e wo/ having NDB_HAVE_XCNG) */
extern  int xcng(volatile unsigned * addr, int val);
#endif

#elif defined(__powerpc__)
#define NDB_HAVE_MB
#define NDB_HAVE_RMB
#define NDB_HAVE_WMB
#define NDB_HAVE_XCNG

#define mb() asm volatile("lwsync;" ::: "memory")
#define rmb() asm volatile("lwsync;" ::: "memory")
#define wmb() asm volatile("lwsync;" ::: "memory")

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

#elif defined(__aarch64__)
#include <atomic>
#define NDB_HAVE_MB
#define NDB_HAVE_RMB
#define NDB_HAVE_WMB
//#define NDB_HAVE_XCNG
#define NDB_HAVE_CPU_PAUSE

#define mb() std::atomic_thread_fence(std::memory_order_seq_cst)
#define rmb() std::atomic_thread_fence(std::memory_order_seq_cst)
#define wmb() std::atomic_thread_fence(std::memory_order_seq_cst)

#define cpu_pause()  __asm__ __volatile__ ("yield")

#else
#define NDB_NO_ASM "Unsupported architecture (gcc)"
#endif

#elif defined (_MSC_VER)

#define NDB_HAVE_MB
#define NDB_HAVE_RMB
#define NDB_HAVE_WMB

#include <windows.h>
#define mb()    MemoryBarrier()
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
