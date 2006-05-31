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

#if defined(__i386__) || defined(_M_IX86)
#ifdef MY_ATOMIC_MODE_DUMMY
#  define LOCK ""
#else
#  define LOCK "lock "
#endif
#ifdef __GNUC__
#include "x86-gcc.h"
#elif defined(_MSC_VER)
#include "x86-msvc.h"
#endif
#endif

#ifdef make_atomic_add_body8

#ifdef HAVE_INLINE

#define make_atomic_add(S)					\
static inline uint ## S _my_atomic_add ## S(			\
        my_atomic_ ## S ## _t *a, uint ## S v)			\
{								\
  make_atomic_add_body ## S;					\
  return v;							\
}

#define make_atomic_swap(S)					\
static inline uint ## S _my_atomic_swap ## S(			\
        my_atomic_ ## S ## _t *a, uint ## S v)			\
{								\
  make_atomic_swap_body ## S;					\
  return v;							\
}

#define make_atomic_cas(S)					\
static inline uint _my_atomic_cas ## S(my_atomic_ ## S ## _t *a,\
        uint ## S *cmp, uint ## S set)				\
{								\
  uint8 ret;							\
  make_atomic_cas_body ## S;					\
  return ret;							\
}

#define make_atomic_load(S)					\
static inline uint ## S _my_atomic_load ## S(			\
        my_atomic_ ## S ## _t *a)				\
{								\
  uint ## S ret;						\
  make_atomic_load_body ## S;					\
  return ret;							\
}

#define make_atomic_store(S)					\
static inline void _my_atomic_store ## S(			\
        my_atomic_ ## S ## _t *a, uint ## S v)			\
{								\
  make_atomic_store_body ## S;					\
}

#else /* no inline functions */

#define make_atomic_add(S)					\
extern uint ## S _my_atomic_add ## S(				\
        my_atomic_ ## S ## _t *a, uint ## S v);

#define make_atomic_swap(S)					\
extern uint ## S _my_atomic_swap ## S(				\
        my_atomic_ ## S ## _t *a, uint ## S v);

#define make_atomic_cas(S)					\
extern uint _my_atomic_cas ## S(my_atomic_ ## S ## _t *a,	\
        uint ## S *cmp, uint ## S set);

#define make_atomic_load(S)					\
extern uint ## S _my_atomic_load ## S(				\
        my_atomic_ ## S ## _t *a);

#define make_atomic_store(S)					\
extern void _my_atomic_store ## S(				\
        my_atomic_ ## S ## _t *a, uint ## S v);

#endif

make_atomic_add( 8)
make_atomic_add(16)
make_atomic_add(32)

make_atomic_cas( 8)
make_atomic_cas(16)
make_atomic_cas(32)

make_atomic_load( 8)
make_atomic_load(16)
make_atomic_load(32)

make_atomic_store( 8)
make_atomic_store(16)
make_atomic_store(32)

make_atomic_swap( 8)
make_atomic_swap(16)
make_atomic_swap(32)

#undef make_atomic_add_body8
#undef make_atomic_cas_body8
#undef make_atomic_load_body8
#undef make_atomic_store_body8
#undef make_atomic_swap_body8
#undef make_atomic_add_body16
#undef make_atomic_cas_body16
#undef make_atomic_load_body16
#undef make_atomic_store_body16
#undef make_atomic_swap_body16
#undef make_atomic_add_body32
#undef make_atomic_cas_body32
#undef make_atomic_load_body32
#undef make_atomic_store_body32
#undef make_atomic_swap_body32
#undef make_atomic_add
#undef make_atomic_cas
#undef make_atomic_load
#undef make_atomic_store
#undef make_atomic_swap

#define my_atomic_add8(a,v,L)  _my_atomic_add8(a,v)
#define my_atomic_add16(a,v,L) _my_atomic_add16(a,v)
#define my_atomic_add32(a,v,L) _my_atomic_add32(a,v)

#define my_atomic_cas8(a,c,v,L)  _my_atomic_cas8(a,c,v)
#define my_atomic_cas16(a,c,v,L) _my_atomic_cas16(a,c,v)
#define my_atomic_cas32(a,c,v,L) _my_atomic_cas32(a,c,v)

#define my_atomic_load8(a,L)  _my_atomic_load8(a)
#define my_atomic_load16(a,L) _my_atomic_load16(a)
#define my_atomic_load32(a,L) _my_atomic_load32(a)

#define my_atomic_store8(a,v,L)  _my_atomic_store8(a,v)
#define my_atomic_store16(a,v,L) _my_atomic_store16(a,v)
#define my_atomic_store32(a,v,L) _my_atomic_store32(a,v)

#define my_atomic_swap8(a,v,L)  _my_atomic_swap8(a,v)
#define my_atomic_swap16(a,v,L) _my_atomic_swap16(a,v)
#define my_atomic_swap32(a,v,L) _my_atomic_swap32(a,v)

#define my_atomic_rwlock_t typedef int
#define my_atomic_rwlock_destroy(name)
#define my_atomic_rwlock_init(name)
#define my_atomic_rwlock_rdlock(name)
#define my_atomic_rwlock_wrlock(name)
#define my_atomic_rwlock_rdunlock(name)
#define my_atomic_rwlock_wrunlock(name)

#endif

