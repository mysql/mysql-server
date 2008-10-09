/* Copyright (C) 2003 MySQL AB

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

/**
 * Only memory barriers *must* be ported
 * if XCNG (x86-sematics) is provided, spinlocks will be enabled
 */
#ifndef NDB_MT_ASM_H
#define NDB_MT_ASM_H

#if defined(__GNUC__)
#if defined(__x86_64__)
/* Memory barriers, these definitions are for x64_64. */
#define mb()    asm volatile("mfence":::"memory")
/* According to Intel docs, it does not reorder loads. */
//#define rmb() asm volatile("lfence":::"memory")                               
#define rmb()   asm volatile("" ::: "memory")
#define wmb()   asm volatile("" ::: "memory")
#define read_barrier_depends()  do {} while(0)

#define NDB_HAVE_XCNG
static inline
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
#define mb()    asm volatile("membar #LoadLoad | #LoadStore | #StoreLoad | #StoreStore":::"memory)
#define rmb()   asm volatile("membar #LoadLoad" ::: "memory")
#define wmb()   asm volatile("membar #StoreStore" ::: "memory")
#define read_barrier_depends()  do {} while(0)

// link error if used incorrectly (i.e wo/ having NDB_HAVE_XCNG)
extern  int xcng(volatile unsigned * addr, int val);
extern void cpu_pause();

#elif
#error "Unsupported architecture"
#endif
#else
#error "Unsupported compiler"
#endif

#endif
