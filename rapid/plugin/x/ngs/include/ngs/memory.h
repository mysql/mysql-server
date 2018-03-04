/*
 * Copyright (c) 2015, 2017, Oracle and/or its affiliates. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is also distributed with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have included with MySQL.
 *  
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License, version 2.0, for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
 */

#ifndef _NGS_MEMORY_H_
#define _NGS_MEMORY_H_

#include <mysql/plugin.h>

#include "my_compiler.h"
#include "plugin/x/ngs/include/ngs_common/bind.h"
#include "plugin/x/ngs/include/ngs_common/smart_ptr.h"

namespace ngs
{
extern unsigned int x_psf_objects_key;

namespace detail
{
// PSF instrumented allocator class that can be used with STL objects
template <class T>
class PFS_allocator: public std::allocator<T>
{
public:
  PFS_allocator()
  {}

  template <class U>
  PFS_allocator(PFS_allocator<U> const &)
  {}

  template <class U>
  struct rebind
  {
    typedef PFS_allocator<U> other;
  };

  T *allocate(size_t n, const void *hint MY_ATTRIBUTE((unused)) = 0)
  {
    return reinterpret_cast<T*>
          (my_malloc(x_psf_objects_key, sizeof(T) * n, MYF(MY_WME) ));
  }

  void deallocate(T *ptr, size_t)
  {
    my_free(ptr);
  }
};

} // namespace detail


// instrumented deallocator
template <class T>
void free_object(T *ptr)
{
  if (ptr != NULL)
  {
    ptr->~T();
    my_free(ptr);
  }
}


// set of instrumented object allocators for different parameters number
template <typename T>
T *allocate_object()
{
  return new ( my_malloc( x_psf_objects_key, sizeof(T), MYF(MY_WME) ) ) T();
}


template <typename T, typename Arg1>
T *allocate_object(Arg1 const &arg1)
{
  return new ( my_malloc( x_psf_objects_key, sizeof(T), MYF(MY_WME) ) ) T(arg1);
}


template <typename T, typename Arg1, typename Arg2>
T *allocate_object(Arg1 const &arg1, Arg2 const &arg2)
{
  return new ( my_malloc( x_psf_objects_key, sizeof(T), MYF(MY_WME) ) ) T(arg1, arg2);
}


template <typename T, typename Arg1, typename Arg2, typename Arg3>
T *allocate_object(Arg1 const &arg1, Arg2 const &arg2, Arg3 const &arg3)
{
  return new ( my_malloc( x_psf_objects_key, sizeof(T), MYF(MY_WME) ) ) T(arg1, arg2, arg3);
}


template <typename T, typename Arg1, typename Arg2, typename Arg3, typename Arg4>
T *allocate_object(Arg1 const &arg1, Arg2 const &arg2, Arg3 const &arg3, Arg4 const &arg4)
{
  return new ( my_malloc( x_psf_objects_key, sizeof(T), MYF(MY_WME) ) ) T(arg1, arg2, arg3, arg4);
}

template <typename T, typename Arg1, typename Arg2, typename Arg3, typename Arg4, typename Arg5>
T *allocate_object(Arg1 const &arg1, Arg2 const &arg2, Arg3 const &arg3, Arg4 const &arg4, Arg5 const &arg5)
{
  return new ( my_malloc( x_psf_objects_key, sizeof(T), MYF(MY_WME) ) ) T(arg1, arg2, arg3, arg4, arg5);
}

template <typename T, typename Arg1, typename Arg2, typename Arg3, typename Arg4, typename Arg5, typename Arg6>
T *allocate_object(Arg1 const &arg1, Arg2 const &arg2, Arg3 const &arg3, Arg4 const &arg4, Arg5 const &arg5, Arg6 const &arg6)
{
  return new ( my_malloc( x_psf_objects_key, sizeof(T), MYF(MY_WME) ) ) T(arg1, arg2, arg3, arg4, arg5, arg6);
}

template <typename T, typename ... Args>
ngs::shared_ptr<T> allocate_shared(Args&&... args) {
  return ngs::detail::allocate_shared<T>(detail::PFS_allocator<T>(),
      std::forward<Args>(args)...);
}


// allocates array of selected type using mysql server instrumentation
template <typename ArrayType>
void allocate_array(ArrayType *&array_ptr, std::size_t size, unsigned int psf_key)
{
  array_ptr = reinterpret_cast<ArrayType*>(my_malloc(psf_key, sizeof(ArrayType)*size, 0));
}


// reallocates array of selected type using mysql server instrumentation
// does simple allocate if null pointer passed
template <typename ArrayType>
void reallocate_array(ArrayType*& array_ptr, std::size_t size, unsigned int psf_key)
{
  if (NULL == array_ptr)
  {
    ngs::allocate_array(array_ptr, size, psf_key);
    return;
  }

  array_ptr = reinterpret_cast<ArrayType*>(my_realloc(psf_key, array_ptr, sizeof(ArrayType)*size, 0));
}


// frees array of selected type using mysql server instrumentation
template <typename ArrayType>
void free_array(ArrayType *array_ptr)
{
  my_free(array_ptr);
}


// wrapper for ngs unique ptr with instrumented default deallocator
template<typename Type>
struct Memory_instrumented
{
  struct Unary_delete
  {
    void operator() (Type *ptr)
    {
      free_object(ptr);
    }
  };

  typedef ngs::unique_ptr<Type, Unary_delete> Unique_ptr;
};


// PSF instrumented string
typedef std::basic_string<char, std::char_traits<char>, detail::PFS_allocator<char> > PFS_string;

} // namespace ngs

#endif // _NGS_MEMORY_H_
