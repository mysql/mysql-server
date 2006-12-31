/* Copyright (C) 2004-2005 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */

#include <ndb_global.h>
#include <NdbMem.h>

extern "C" {
  void (* ndb_new_handler)() = 0;
}

#if 0

void *operator new (size_t sz)
{
  void * p = NdbMem_Allocate(sz ? sz : 1);
  if(p)
    return p;
  if(ndb_new_handler)
    (* ndb_new_handler)();
  abort();
}

void *operator new[] (size_t sz)
{
  void * p = (void *) NdbMem_Allocate(sz ? sz : 1);
  if(p)
    return p;
  if(ndb_new_handler)
    (* ndb_new_handler)();
  abort();
}

void operator delete (void *ptr)
{
  if (ptr)
    NdbMem_Free(ptr);
}

void operator delete[] (void *ptr) throw ()
{
  if (ptr)
    NdbMem_Free(ptr);
}

#endif // USE_MYSYS_NEW
