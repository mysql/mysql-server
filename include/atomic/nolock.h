#ifndef ATOMIC_NOLOCK_INCLUDED
#define ATOMIC_NOLOCK_INCLUDED

/* Copyright (c) 2006, 2010, Oracle and/or its affiliates. All rights reserved.

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

#if defined(__i386__) || defined(_MSC_VER) || defined(__x86_64__)   \
    || defined(HAVE_GCC_ATOMIC_BUILTINS) \
    || defined(HAVE_SOLARIS_ATOMIC)

#  ifdef MY_ATOMIC_MODE_DUMMY
#    define LOCK_prefix ""
#  else
#    define LOCK_prefix "lock"
#  endif
/*
  We choose implementation as follows:
  ------------------------------------
  On Windows using Visual C++ the native implementation should be
  preferrable. When using gcc we prefer the Solaris implementation
  before the gcc because of stability preference, we choose gcc
  builtins if available, otherwise we choose the somewhat broken
  native x86 implementation. If neither Visual C++ or gcc we still
  choose the Solaris implementation on Solaris (mainly for SunStudio
  compilers).
*/
#  if defined(_MSC_VER)
#    include "generic-msvc.h"
#  elif __GNUC__
#    if defined(HAVE_SOLARIS_ATOMIC)
#      include "solaris.h"
#    elif defined(HAVE_GCC_ATOMIC_BUILTINS)
#      include "gcc_builtins.h"
#    elif defined(__i386__) || defined(__x86_64__)
#      include "x86-gcc.h"
#    endif
#  elif defined(HAVE_SOLARIS_ATOMIC)
#    include "solaris.h"
#  endif
#endif

#if defined(make_atomic_cas_body)
/*
  Type not used so minimal size (emptry struct has different size between C
  and C++, zero-length array is gcc-specific).
*/
typedef char my_atomic_rwlock_t __attribute__ ((unused));
#define my_atomic_rwlock_destroy(name)
#define my_atomic_rwlock_init(name)
#define my_atomic_rwlock_rdlock(name)
#define my_atomic_rwlock_wrlock(name)
#define my_atomic_rwlock_rdunlock(name)
#define my_atomic_rwlock_wrunlock(name)

#endif

#endif /* ATOMIC_NOLOCK_INCLUDED */
