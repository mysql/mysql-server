/* Copyright (c) 2016, Oracle and/or its affiliates. All rights reserved.

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

#include "keyring_memory.h"


#ifdef _WIN32
void* operator new(size_t size)
#else
void* operator new(size_t size) throw(std::bad_alloc)
#endif
{
  void *memory= keyring::keyring_malloc<void*>(size);
  if (memory == NULL)
    throw std::bad_alloc();
  return memory;
}

#ifdef _WIN32
void operator delete(void* ptr)
#else
void operator delete(void* ptr) throw()
#endif
{
  my_free(ptr);
}

#ifdef _WIN32
void* operator new[] (size_t size)
#else
void* operator new[] (size_t size) throw(std::bad_alloc)
#endif
{
  return operator new(size);
}

#ifdef _WIN32
void operator delete[] (void* ptr)
#else
void operator delete[] (void* ptr) throw()
#endif
{
  return operator delete(ptr);
}

