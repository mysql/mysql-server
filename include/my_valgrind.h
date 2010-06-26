/* Copyright (C) 2010 Monty Program Ab

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


/* Some defines to make it easier to use valgrind */

#ifdef HAVE_valgrind
#define IF_VALGRIND(A,B) (A)
#else
#define IF_VALGRIND(A,B) (B)
#endif

#if defined(HAVE_valgrind)&& defined(HAVE_VALGRIND_MEMCHECK_H)
#include <valgrind/memcheck.h>
#else
#define VALGRIND_MAKE_MEM_DEFINED(addr, size) do { } while(0)
#define VALGRIND_MAKE_MEM_NOACCESS(addr, size) do { } while(0)
#ifdef SAFEMALLOC
#define VALGRIND_MAKE_MEM_UNDEFINED(addr, size) bfill(addr, size, 0x8F)
#else
#define VALGRIND_MAKE_MEM_UNDEFINED(addr, size) do { } while(0)
#endif /* SAFEMALLOC */
#endif /* HAVE_valgrind */

/* Compatibility with old source */
#define TRASH(A,B) VALGRIND_MAKE_MEM_UNDEFINED(A, B)
