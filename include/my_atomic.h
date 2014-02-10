#ifndef MY_ATOMIC_INCLUDED
#define MY_ATOMIC_INCLUDED

/* Copyright (c) 2006, 2014, Oracle and/or its affiliates. All rights reserved.

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

/*
  This header defines five atomic operations:

  my_atomic_add#(&var, what)
    'Fetch and Add'
    add 'what' to *var, and return the old value of *var

  my_atomic_fas#(&var, what)
    'Fetch And Store'
    store 'what' in *var, and return the old value of *var

  my_atomic_cas#(&var, &old, new)
    An odd variation of 'Compare And Set/Swap'
    if *var is equal to *old, then store 'new' in *var, and return TRUE
    otherwise store *var in *old, and return FALSE
    Usually, &old should not be accessed if the operation is successful.

  my_atomic_load#(&var)
    return *var

  my_atomic_store#(&var, what)
    store 'what' in *var

  '#' is substituted by a size suffix - 32, 64, or ptr
  (e.g. my_atomic_add64, my_atomic_fas32, my_atomic_casptr).

  NOTE This operations are not always atomic, so they always must be
  enclosed in my_atomic_rwlock_rdlock(lock)/my_atomic_rwlock_rdunlock(lock)
  or my_atomic_rwlock_wrlock(lock)/my_atomic_rwlock_wrunlock(lock).
  Hint: if a code block makes intensive use of atomic ops, it make sense
  to take/release rwlock once for the whole block, not for every statement.

  On architectures where these operations are really atomic, rwlocks will
  be optimized away.
*/

/*
  Attempt to do atomic ops without locks

  We choose implementation as follows:
  ------------------------------------
  On Windows using Visual C++ the native implementation should be
  preferrable. When using gcc we prefer the Solaris implementation
  before the gcc because of stability preference, we choose gcc
  builtins if available, otherwise we fallback to rw locks. If
  neither Visual C++ or gcc we still choose the Solaris
  implementation on Solaris (mainly for SunStudio compilers).
*/
#ifndef MY_ATOMIC_MODE_RWLOCKS
#  if defined(_MSC_VER)
#    include "atomic/generic-msvc.h"
#  elif defined(HAVE_SOLARIS_ATOMIC)
#    include "atomic/solaris.h"
#  elif defined(HAVE_GCC_ATOMIC_BUILTINS)
#    include "atomic/gcc_builtins.h"
#  else
#    define MY_ATOMIC_MODE_RWLOCKS 1
#  endif
#endif /* !MY_ATOMIC_MODE_RWLOCKS */

#ifdef MY_ATOMIC_MODE_RWLOCKS
#  include "atomic/rwlock.h"
#else
typedef char my_atomic_rwlock_t __attribute__ ((unused));
#  define my_atomic_rwlock_destroy(name)
#  define my_atomic_rwlock_init(name)
#  define my_atomic_rwlock_rdlock(name)
#  define my_atomic_rwlock_wrlock(name)
#  define my_atomic_rwlock_rdunlock(name)
#  define my_atomic_rwlock_wrunlock(name)
#endif /* MY_ATOMIC_MODE_RWLOCKS */

/*
  the macro below defines (as an expression) the code that
  will be run in spin-loops. Intel manuals recummend to have PAUSE there.
  It is expected to be defined in include/atomic/ *.h files
*/
#ifndef LF_BACKOFF
#define LF_BACKOFF (1)
#endif

#endif /* MY_ATOMIC_INCLUDED */
