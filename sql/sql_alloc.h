#ifndef SQL_ALLOC_INCLUDED
#define SQL_ALLOC_INCLUDED
/* Copyright (c) 2012, 2017, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

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
