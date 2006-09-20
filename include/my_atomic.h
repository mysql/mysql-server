/* Copyright (C) 2006 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#ifndef my_atomic_rwlock_init

#define intptr         void *

#ifndef MY_ATOMIC_MODE_RWLOCKS
#include "atomic/nolock.h"
#endif

#ifndef make_atomic_cas_body
#include "atomic/rwlock.h"
#endif

#ifndef make_atomic_add_body
#define make_atomic_add_body(S)					\
  int ## S tmp=*a;                                              \
  while (!my_atomic_cas ## S(a, &tmp, tmp+v));                  \
  v=tmp;
#endif

#ifdef HAVE_INLINE

#define make_atomic_add(S)					\
STATIC_INLINE int ## S my_atomic_add ## S(			\
                        int ## S volatile *a, int ## S v)	\
{								\
  make_atomic_add_body(S);					\
  return v;							\
}

#define make_atomic_swap(S)					\
STATIC_INLINE int ## S my_atomic_swap ## S(			\
                         int ## S volatile *a, int ## S v)	\
{								\
  make_atomic_swap_body(S);					\
  return v;							\
}

#define make_atomic_cas(S)					\
STATIC_INLINE int my_atomic_cas ## S(int ## S volatile *a,	\
                            int ## S *cmp, int ## S set)	\
{								\
  int8 ret;							\
  make_atomic_cas_body(S);					\
  return ret;							\
}

#define make_atomic_load(S)					\
STATIC_INLINE int ## S my_atomic_load ## S(int ## S volatile *a) \
{								\
  int ## S ret;						\
  make_atomic_load_body(S);					\
  return ret;							\
}

#define make_atomic_store(S)					\
STATIC_INLINE void my_atomic_store ## S(			\
                     int ## S volatile *a, int ## S v)	\
{								\
  make_atomic_store_body(S);					\
}

#else /* no inline functions */

#define make_atomic_add(S)					\
extern int ## S my_atomic_add ## S(int ## S volatile *a, int ## S v);

#define make_atomic_swap(S)					\
extern int ## S my_atomic_swap ## S(int ## S volatile *a, int ## S v);

#define make_atomic_cas(S)					\
extern int my_atomic_cas ## S(int ## S volatile *a, int ## S *cmp, int ## S set);

#define make_atomic_load(S)					\
extern int ## S my_atomic_load ## S(int ## S volatile *a);

#define make_atomic_store(S)					\
extern void my_atomic_store ## S(int ## S volatile *a, int ## S v);

#endif

make_atomic_cas( 8)
make_atomic_cas(16)
make_atomic_cas(32)
make_atomic_cas(ptr)

make_atomic_add( 8)
make_atomic_add(16)
make_atomic_add(32)

make_atomic_load( 8)
make_atomic_load(16)
make_atomic_load(32)
make_atomic_load(ptr)

make_atomic_store( 8)
make_atomic_store(16)
make_atomic_store(32)
make_atomic_store(ptr)

make_atomic_swap( 8)
make_atomic_swap(16)
make_atomic_swap(32)
make_atomic_swap(ptr)

#undef make_atomic_add
#undef make_atomic_cas
#undef make_atomic_load
#undef make_atomic_store
#undef make_atomic_swap
#undef make_atomic_add_body
#undef make_atomic_cas_body
#undef make_atomic_load_body
#undef make_atomic_store_body
#undef make_atomic_swap_body
#undef intptr

#ifdef _atomic_h_cleanup_
#include _atomic_h_cleanup_
#undef _atomic_h_cleanup_
#endif

#ifndef LF_BACKOFF
#define LF_BACKOFF (1)
#endif

#if SIZEOF_CHARP == SIZEOF_INT
typedef int intptr;
#elif SIZEOF_CHARP == SIZEOF_LONG
typedef long intptr;
#else
#error
#endif

#define MY_ATOMIC_OK       0
#define MY_ATOMIC_NOT_1CPU 1
extern int my_atomic_initialize();

#endif

