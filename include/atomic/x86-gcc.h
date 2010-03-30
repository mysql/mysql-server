#ifndef ATOMIC_X86_GCC_INCLUDED
#define ATOMIC_X86_GCC_INCLUDED

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

/*
  No special support of 8 and 16 bit operations are implemented here
  currently.
*/
#undef MY_ATOMIC_HAS_8_AND_16

#ifdef __x86_64__
#  ifdef MY_ATOMIC_NO_XADD
#    define MY_ATOMIC_MODE "gcc-amd64" LOCK_prefix "-no-xadd"
#  else
#    define MY_ATOMIC_MODE "gcc-amd64" LOCK_prefix
#  endif
#else
#  ifdef MY_ATOMIC_NO_XADD
#    define MY_ATOMIC_MODE "gcc-x86" LOCK_prefix "-no-xadd"
#  else
#    define MY_ATOMIC_MODE "gcc-x86" LOCK_prefix
#  endif
#endif

/* fix -ansi errors while maintaining readability */
#ifndef asm
#define asm __asm__
#endif

#ifndef MY_ATOMIC_NO_XADD
#define make_atomic_add_body(S)         make_atomic_add_body ## S
#define make_atomic_cas_body(S)         make_atomic_cas_body ## S
#endif

#define make_atomic_add_body32                                  \
  asm volatile (LOCK_prefix "; xadd %0, %1;" : "+r" (v) , "+m" (*a))

#define make_atomic_cas_body32                                  \
  asm volatile (LOCK_prefix "; cmpxchg %3, %0; setz %2;"	\
               : "+m" (*a), "+a" (*cmp), "=q" (ret): "r" (set))

#ifdef __x86_64__
#define make_atomic_add_body64 make_atomic_add_body32
#define make_atomic_cas_body64 make_atomic_cas_body32

#define make_atomic_fas_body(S)                                        \
  asm volatile ("xchg %0, %1;" : "+r" (v) , "+m" (*a))

/*
  Actually 32-bit reads/writes are always atomic on x86
  But we add LOCK_prefix here anyway to force memory barriers
*/
#define make_atomic_load_body(S)                                \
  ret=0;                                                        \
  asm volatile (LOCK_prefix "; cmpxchg %2, %0"                  \
                : "+m" (*a), "+a" (ret): "r" (ret))
#define make_atomic_store_body(S)                               \
  asm volatile ("; xchg %0, %1;" : "+m" (*a), "+r" (v))

#else
/*
  Use default implementations of 64-bit operations since we solved
  the 64-bit problem on 32-bit platforms for CAS, no need to solve it
  once more for ADD, LOAD, STORE and FAS as well.
  Since we already added add32 support, we need to define add64
  here, but we haven't defined fas, load and store at all, so
  we can fallback on default implementations.
*/
#define make_atomic_add_body64                                  \
  int64 tmp=*a;                                                 \
  while (!my_atomic_cas64(a, &tmp, tmp+v)) ;                    \
  v=tmp;

/*
  On some platforms (e.g. Mac OS X and Solaris) the ebx register
  is held as a pointer to the global offset table. Thus we're not
  allowed to use the b-register on those platforms when compiling
  PIC code, to avoid this we push ebx and pop ebx and add a movl
  instruction to avoid having ebx in the interface of the assembler
  instruction.

  cmpxchg8b works on both 32-bit platforms and 64-bit platforms but
  the code here is only used on 32-bit platforms, on 64-bit
  platforms the much simpler make_atomic_cas_body32 will work
  fine.
*/
#define make_atomic_cas_body64                                  \
  int32 ebx=(set & 0xFFFFFFFF), ecx=(set >> 32);                \
  asm volatile ("push %%ebx; movl %3, %%ebx;"                   \
                LOCK_prefix "; cmpxchg8b %0; setz %2; pop %%ebx"\
               : "+m" (*a), "+A" (*cmp), "=c" (ret)             \
               :"m" (ebx), "c" (ecx))
#endif

/*
  The implementation of make_atomic_cas_body32 is adaptable to
  the OS word size, so on 64-bit platforms it will automatically
  adapt to 64-bits and so it will work also on 64-bit platforms
*/
#define make_atomic_cas_bodyptr make_atomic_cas_body32

#ifdef MY_ATOMIC_MODE_DUMMY
#define make_atomic_load_body(S)   ret=*a
#define make_atomic_store_body(S)  *a=v
#endif
#endif /* ATOMIC_X86_GCC_INCLUDED */
