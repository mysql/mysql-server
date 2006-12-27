/* Copyright (C) 2006 MySQL AB

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

/*
  XXX 64-bit atomic operations can be implemented using
  cmpxchg8b, if necessary. Though I've heard that not all 64-bit
  architectures support double-word (128-bit) cas.
*/

#ifdef MY_ATOMIC_NO_XADD
#define MY_ATOMIC_MODE "gcc-x86" LOCK "-no-xadd"
#else
#define MY_ATOMIC_MODE "gcc-x86" LOCK
#endif

/* fix -ansi errors while maintaining readability */
#ifndef asm
#define asm __asm__
#endif

#ifndef MY_ATOMIC_NO_XADD
#define make_atomic_add_body(S)					\
  asm volatile (LOCK "; xadd %0, %1;" : "+r" (v) , "+m" (*a))
#endif
#define make_atomic_swap_body(S)				\
  asm volatile ("; xchg %0, %1;" : "+r" (v) , "+m" (*a))
#define make_atomic_cas_body(S)					\
  asm volatile (LOCK "; cmpxchg %3, %0; setz %2;"		\
               : "+m" (*a), "+a" (*cmp), "=q" (ret): "r" (set))

#ifdef MY_ATOMIC_MODE_DUMMY
#define make_atomic_load_body(S)   ret=*a
#define make_atomic_store_body(S)  *a=v
#else
/*
  Actually 32-bit reads/writes are always atomic on x86
  But we add LOCK here anyway to force memory barriers
*/
#define make_atomic_load_body(S)				\
  ret=0;							\
  asm volatile (LOCK "; cmpxchg %2, %0"				\
               : "+m" (*a), "+a" (ret): "r" (ret))
#define make_atomic_store_body(S)				\
  asm volatile ("; xchg %0, %1;" : "+m" (*a) : "r" (v))
#endif

