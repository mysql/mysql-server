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
#  define LOCK "lock"
#endif

#ifdef __GNUC__
#include "x86-gcc.h"
#elif defined(_MSC_VER)
#include "x86-msvc.h"
#endif
#endif

#ifdef make_atomic_cas_body

typedef struct { } my_atomic_rwlock_t;
#define my_atomic_rwlock_destroy(name)
#define my_atomic_rwlock_init(name)
#define my_atomic_rwlock_rdlock(name)
#define my_atomic_rwlock_wrlock(name)
#define my_atomic_rwlock_rdunlock(name)
#define my_atomic_rwlock_wrunlock(name)

#endif

