/*
   Copyright (c) 2003, 2010, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef NDB_PREFETCH_H
#define NDB_PREFETCH_H

#ifdef HAVE_SUN_PREFETCH_H
#include <sun_prefetch.h>
#if (defined(__SUNPRO_C) && __SUNPRO_C >= 0x590) \
    || (defined(__SUNPRO_CC) && __SUNPRO_CC >= 0x590)
/* Universal sun_prefetch* macros available with Sun Studio 5.9 */
#define USE_SUN_PREFETCH
#elif defined(__sparc)
/* Use sparc_prefetch* macros with older Sun Studio on sparc */
#define USE_SPARC_PREFETCH
#endif
#endif

#ifdef HAVE_SUN_PREFETCH_H
#pragma optimize("", off)
#endif

static inline
void NDB_PREFETCH_READ(void* addr)
{
#if (__GNUC__ > 3) || (__GNUC__ == 3 && __GNUC_MINOR > 10)
  __builtin_prefetch(addr, 0, 3);
#elif defined(USE_SUN_PREFETCH)
  sun_prefetch_read_once(addr);
#elif defined(USE_SPARC_PREFETCH)
  sparc_prefetch_read_once(addr);
#else
  (void)addr;
#endif
}

static inline
void NDB_PREFETCH_WRITE(void* addr)
{
#if (__GNUC__ > 3) || (__GNUC__ == 3 && __GNUC_MINOR > 10)
  __builtin_prefetch(addr, 1, 3);
#elif defined(USE_SUN_PREFETCH)
  sun_prefetch_write_once(addr);
#elif defined(USE_SPARC_PREFETCH)
  sparc_prefetch_write_once(addr);
#else
  (void)addr;
#endif
}

#ifdef HAVE_SUN_PREFETCH_H
#pragma optimize("", on)
#endif

#endif

