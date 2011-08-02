/* Copyright (c) 2008, 2010, Oracle and/or its affiliates. All rights reserved.

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

#ifndef _atomic_h_cleanup_
#define _atomic_h_cleanup_ "atomic/solaris.h"

#include <atomic.h>

#define	MY_ATOMIC_MODE	"solaris-atomic"

#if defined(__GNUC__)
#define atomic_typeof(T,V)      __typeof__(V)
#else
#define atomic_typeof(T,V)      T
#endif

#define uintptr_t void *
#define atomic_or_ptr_nv(X,Y) (void *)atomic_or_ulong_nv((volatile ulong_t *)X, Y)

#define make_atomic_cas_body(S)                         \
  atomic_typeof(uint ## S ## _t, *cmp) sav;             \
  sav = atomic_cas_ ## S(                               \
           (volatile uint ## S ## _t *)a,               \
           (uint ## S ## _t)*cmp,                       \
           (uint ## S ## _t)set);                       \
  if (! (ret= (sav == *cmp)))                           \
    *cmp= sav;

#define make_atomic_add_body(S)                         \
  int ## S nv;  /* new value */                         \
  nv= atomic_add_ ## S ## _nv((volatile uint ## S ## _t *)a, v); \
  v= nv - v

/* ------------------------------------------------------------------------ */

#ifdef MY_ATOMIC_MODE_DUMMY

#define make_atomic_load_body(S)  ret= *a
#define make_atomic_store_body(S)   *a= v

#else /* MY_ATOMIC_MODE_DUMMY */

#define make_atomic_load_body(S)                        \
  ret= atomic_or_ ## S ## _nv((volatile uint ## S ## _t *)a, 0)

#define make_atomic_store_body(S)                       \
  (void) atomic_swap_ ## S((volatile uint ## S ## _t *)a, (uint ## S ## _t)v)

#endif

#define make_atomic_fas_body(S)                        \
  v= atomic_swap_ ## S((volatile uint ## S ## _t *)a, (uint ## S ## _t)v)

#else /* cleanup */

#undef uintptr_t
#undef atomic_or_ptr_nv

#endif

