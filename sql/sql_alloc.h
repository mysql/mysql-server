#ifndef SQL_ALLOC_INCLUDED
#define SQL_ALLOC_INCLUDED
/* Copyright (c) 2012, 2016, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

#include <new>

#include "my_sys.h"         // TRASH
#include "thr_malloc.h"     // alloc_root

/**
  MySQL standard memory allocator class. You have to inherit the class
  in order to use it.
*/
class Sql_alloc
{
public:
  static void *operator new(size_t size) throw ()
  {
    return sql_alloc(size);
  }
  static void *operator new[](size_t size) throw ()
  {
    return sql_alloc(size);
  }
  static void *operator new[](size_t size, MEM_ROOT *mem_root,
                              const std::nothrow_t &arg MY_ATTRIBUTE((unused))
                              = std::nothrow) throw ()
  { return alloc_root(mem_root, size); }

  static void *operator new(size_t size, MEM_ROOT *mem_root,
                            const std::nothrow_t &arg MY_ATTRIBUTE((unused))
                            = std::nothrow) throw ()
  { return alloc_root(mem_root, size); }

  static void operator delete(void *ptr MY_ATTRIBUTE((unused)),
                              size_t size MY_ATTRIBUTE((unused)))
  { TRASH(ptr, size); }
  static void operator delete(void*, MEM_ROOT*,
                              const std::nothrow_t&) throw ()
  { /* never called */ }
  static void operator delete[](void*, MEM_ROOT*,
                                const std::nothrow_t&) throw ()
  { /* never called */ }
  static void operator delete[](void *ptr MY_ATTRIBUTE((unused)),
                                size_t size MY_ATTRIBUTE((unused)))
  { TRASH(ptr, size); }

  inline Sql_alloc() {}
  inline ~Sql_alloc() {}
};

#endif // SQL_ALLOC_INCLUDED
