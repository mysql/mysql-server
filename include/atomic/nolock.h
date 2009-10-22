#ifndef ATOMIC_NOLOCK_INCLUDED
#define ATOMIC_NOLOCK_INCLUDED

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

#if defined(__i386__) || defined(_M_IX86) || defined(HAVE_GCC_ATOMIC_BUILTINS)

#ifdef MY_ATOMIC_MODE_DUMMY
#  define LOCK ""
#else
#  define LOCK "lock"
#endif

#ifdef HAVE_GCC_ATOMIC_BUILTINS
#include "gcc_builtins.h"
#elif __GNUC__
#include "x86-gcc.h"
#elif defined(_MSC_VER)
#include "x86-msvc.h"
#endif

#elif defined(HAVE_SOLARIS_ATOMIC)

#include "solaris.h"

#endif /* __i386__ || _M_IX86 || HAVE_GCC_ATOMIC_BUILTINS */

#if defined(make_atomic_cas_body) || defined(MY_ATOMICS_MADE)
/*
 * We have atomics that require no locking
 */
#define	MY_ATOMIC_NOLOCK

#ifdef __SUNPRO_C
/*
 * Sun Studio 12 (and likely earlier) does not accept a typedef struct {}
 */
typedef char my_atomic_rwlock_t;
#else
typedef struct { } my_atomic_rwlock_t;
#endif

#define my_atomic_rwlock_destroy(name)
#define my_atomic_rwlock_init(name)
#define my_atomic_rwlock_rdlock(name)
#define my_atomic_rwlock_wrlock(name)
#define my_atomic_rwlock_rdunlock(name)
#define my_atomic_rwlock_wrunlock(name)

#endif

#endif /* ATOMIC_NOLOCK_INCLUDED */
