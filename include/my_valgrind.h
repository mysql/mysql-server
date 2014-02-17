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

#ifdef HAVE_valgrind
#define IF_VALGRIND(A,B) A
#else
#define IF_VALGRIND(A,B) B
#endif

#if defined(HAVE_VALGRIND) && defined(HAVE_valgrind)
# include <valgrind/memcheck.h>
# define MEM_UNDEFINED(a,len) VALGRIND_MAKE_MEM_UNDEFINED(a,len)
# define MEM_NOACCESS(a,len) VALGRIND_MAKE_MEM_NOACCESS(a,len)
# define MEM_CHECK_ADDRESSABLE(a,len) VALGRIND_CHECK_MEM_IS_ADDRESSABLE(a,len)
# define MEM_CHECK_DEFINED(a,len) VALGRIND_CHECK_MEM_IS_DEFINED(a,len)
#else /* HAVE_VALGRIND */
# define MEM_UNDEFINED(a,len) ((void) 0)
# define MEM_NOACCESS(a,len) ((void) 0)
# define MEM_CHECK_ADDRESSABLE(a,len) ((void) 0)
# define MEM_CHECK_DEFINED(a,len) ((void) 0)
#endif /* HAVE_VALGRIND */

#ifndef DBUG_OFF
#define TRASH_FILL(A,B,C) do { memset(A, C, B); MEM_UNDEFINED(A, B); } while (0)
#else
#define TRASH_FILL(A,B,C) do{ MEM_CHECK_ADDRESSABLE(A,B);MEM_UNDEFINED(A,B);} while (0)
#endif
#define TRASH_ALLOC(A,B) TRASH_FILL(A,B,0xA5)
#define TRASH_FREE(A,B) TRASH_FILL(A,B,0x8F)
#define TRASH(A,B) TRASH_FREE(A,B)

