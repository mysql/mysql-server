/*
 * Copyright (c) 2015, 2016, Oracle and/or its affiliates. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; version 2 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301  USA
 */

#ifndef _NGS_MEMORY_H_
#define _NGS_MEMORY_H_

#include <boost/interprocess/smart_ptr/unique_ptr.hpp>
#include <boost/function.hpp>
#include <boost/none.hpp>

template <typename ArrayType>
void Memory_delete_array(ArrayType* array_ptr)
{
  delete[] array_ptr;
}

template <typename Type>
void Memory_delete(Type* ptr)
{
  delete ptr;
}


template <typename Type, typename DeleterType = boost::function<void (Type* value_ptr)> >
struct Custom_allocator
{
  typedef boost::interprocess::unique_ptr<Type, DeleterType > Unique_ptr;
};

template<typename Type>
struct Custom_allocator_with_check
{
  typedef void (*functor_type)(Type *ptr);

  Custom_allocator_with_check()
  {
    function = Memory_delete<Type>;
  }

  Custom_allocator_with_check(const boost::none_t &)
  {
    function = NULL;
  }

  Custom_allocator_with_check(functor_type user_function)
  {
    function = user_function;
  }

  void operator() (Type *ptr)
  {
    if (function)
    {
      function(ptr);
    }
  }

  functor_type function;

  typedef boost::interprocess::unique_ptr<Type, Custom_allocator_with_check<Type> > Unique_ptr;
};

template<typename Type>
struct Memory_new
{
  struct Unary_delete
  {
    void operator() (Type *ptr)
    {
      delete ptr;
    }
  };

  typedef boost::interprocess::unique_ptr<Type, Unary_delete > Unique_ptr;
};

#endif // _NGS_MEMORY_H_
