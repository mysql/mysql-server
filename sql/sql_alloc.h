#ifndef SQL_ALLOC_INCLUDED
#define SQL_ALLOC_INCLUDED
/* Copyright (c) 2012, 2017, Oracle and/or its affiliates. All rights reserved.

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
#include "sql/thr_malloc.h" // alloc_root

/**
  MySQL standard memory allocator class. You have to inherit the class
  in order to use it.

  Objects inheriting from Sql_alloc will automatically be allocated on
  current_thd. That is, if you have a class Foo inheriting from Sql_alloc,
  then

    Foo *foo= new Foo(bar, baz);

  will be equivalent to

    Foo *foo= new (*THR_MALLOC) Foo(bar, baz);

  which in turn is equivalent to

    Foo *foo= static_cast<Foo *>(sql_alloc(*THR_MALLOC, sizeof(Foo)));
    new (foo) Foo(bar, baz);

  In particular, this means that contrary to requirements of the C++ standard,
  new on such objects can return nullptr.

  Note that there is a global user-defined operator new on MEM_ROOT in
  place, so that you don't need to inherit from Sql_alloc to use the second
  form above.
*/
class Sql_alloc
{
public:
  Sql_alloc() {}
  ~Sql_alloc() {}

  static void operator delete(void*, MEM_ROOT*,
                              const std::nothrow_t&) throw ()
  { /* never called */ }
  static void operator delete[](void*, MEM_ROOT*,
                                const std::nothrow_t&) throw ()
  { /* never called */ }
  static void operator delete[](void *ptr MY_ATTRIBUTE((unused)),
                                size_t size MY_ATTRIBUTE((unused)))
  { TRASH(ptr, size); }

  // Duplications of ::operator new from my_alloc.h.
  static void *operator new[](size_t size, MEM_ROOT *mem_root,
                              const std::nothrow_t &arg MY_ATTRIBUTE((unused))
                              = std::nothrow) throw ()
  { return alloc_root(mem_root, size); }

  static void *operator new(size_t size, MEM_ROOT *mem_root,
                            const std::nothrow_t &arg MY_ATTRIBUTE((unused))
                            = std::nothrow) throw ()
  { return alloc_root(mem_root, size); }

protected:
  static void operator delete(void *ptr MY_ATTRIBUTE((unused)),
                              size_t size MY_ATTRIBUTE((unused)))
  { TRASH(ptr, size); }
};

#endif // SQL_ALLOC_INCLUDED
