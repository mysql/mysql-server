/* Copyright (c) 2000, 2001, 2003-2006 MySQL AB, 2009 Sun Microsystems, Inc.
   Use is subject to license terms.

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
  This is a replacement of new/delete operators to be used when compiling
  with gcc 3.0.x to avoid including libstdc++ 
  
  It is also used to make all memory allocations to go through
  my_malloc/my_free wrappers (for debugging/safemalloc and accounting)
*/

#include "mysys_priv.h"
#include <new>

#ifdef USE_MYSYS_NEW

void *operator new (size_t sz)
{
  return (void *) my_malloc (sz ? sz : 1, MYF(0));
}

void *operator new[] (size_t sz)
{
  return (void *) my_malloc (sz ? sz : 1, MYF(0));
}

void* operator new(std::size_t sz, const std::nothrow_t&) throw()
{
  return (void *) my_malloc (sz ? sz : 1, MYF(0));
}

void* operator new[](std::size_t sz, const std::nothrow_t&) throw()
{
  return (void *) my_malloc (sz ? sz : 1, MYF(0));
}

void operator delete (void *ptr)
{
  my_free(ptr);
}

void operator delete[] (void *ptr) throw ()
{
  my_free(ptr);
}

void operator delete(void* ptr, const std::nothrow_t&) throw()
{
  my_free(ptr);
}

void operator delete[](void* ptr, const std::nothrow_t&) throw()
{
  my_free(ptr);
}

C_MODE_START

int __cxa_pure_virtual()
{
  assert(! "Aborted: pure virtual method called.");
  return 0;
}

C_MODE_END
#else
/* 
  Define a dummy symbol, just to avoid compiler/linker warnings
  about compiling an essentially empty file.
*/
int my_new_cc_symbol;
#endif /* USE_MYSYS_NEW */

