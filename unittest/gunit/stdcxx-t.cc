/* Copyright (c) 2011, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02111-1307  USA */

// First include (the generated) my_config.h, to get correct platform defines,
// then gtest.h (before any other MySQL headers), to avoid min() macros etc ...
#include "my_config.h"
#include <gtest/gtest.h>

#if defined(__GNUC__)
#include <tr1/unordered_map>
#elif defined(__WIN__)
#include <hash_map>
#elif  defined(__SUNPRO_CC)
#include <hash_map>
#else 
#error "Don't know how to implement hash_map"
#endif


template<typename K, typename T>
struct MyHashMap
{
#if defined(__GNUC__)
  typedef std::tr1::unordered_map<K, T> Type;
#elif defined(__WIN__)
  typedef stdext::hash_map<K, T> Type;
#elif defined(__SUNPRO_CC)
  typedef std::hash_map<K, T> Type;
#endif
};


TEST(STDfeatures, HashMap)
{
  MyHashMap<int, int>::Type intmap;
  for (int ix= 0; ix < 10; ++ix)
  {
    intmap[ix]= ix * ix;
  }
  int t= intmap[0];
  EXPECT_EQ(0, t);
  EXPECT_TRUE(0 == intmap.count(42));
  EXPECT_TRUE(intmap.end() == intmap.find(42));
}

#if defined(TARGET_OS_LINUX)

#include <malloc.h>

namespace {

void *nop_malloc_hook(size_t size, const void *caller)
{
  return NULL;
}

TEST(StdCxxNoThrow, NoThrow)
{
  typeof(__malloc_hook) old_malloc_hook= __malloc_hook;

  __malloc_hook= nop_malloc_hook;

  const int *pnull= NULL;
  int *ptr= new (std::nothrow) int;
  __malloc_hook= old_malloc_hook;

  EXPECT_EQ(pnull, ptr);
}

TEST(StdCxxExceptionInNew, NewThrowsException)
{
  typeof(__malloc_hook) old_malloc_hook= __malloc_hook;

  __malloc_hook= nop_malloc_hook;

  bool thrown= false;
  try
  {
    int *ptr= new int;
    *ptr= 0;
  }
  catch (std::bad_alloc &e)
  {
    thrown= true;
  }
  __malloc_hook= old_malloc_hook;

  EXPECT_TRUE(thrown);
}

}

#endif // TARGET_OS_LINUX
