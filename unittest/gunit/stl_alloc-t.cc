/* Copyright (c) 2014, Oracle and/or its affiliates. All rights reserved.

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

// First include (the generated) my_config.h, to get correct platform defines.
#include "my_config.h"
#include <gtest/gtest.h>

#include "malloc_allocator.h"
#include "memroot_allocator.h"

#include <vector>
#include <list>
#include <deque>
#include <algorithm>

using std::vector;
using std::list;
using std::deque;

/*
  Tests of custom STL memory allocators.
*/


#if defined(GTEST_HAS_TYPED_TEST)

namespace stlalloc_unittest {

/*
  Wrappers to overcome the issue that we need allocators with
  default constructors for TYPED_TEST_CASE, which neither
  Malloc_allocator nor Memroot_allocator have.

  These wrappers need to inherit so that they are allocators themselves.
  Otherwise TypeParam in the tests below will be wrong.
*/
template<typename T>
class Malloc_allocator_wrapper : public Malloc_allocator<T>
{
public:
  Malloc_allocator_wrapper()
    : Malloc_allocator<T>(PSI_NOT_INSTRUMENTED)
  { }
};

template<typename T>
class Memroot_allocator_wrapper : public Memroot_allocator<T>
{
  MEM_ROOT m_mem_root;

public:
  Memroot_allocator_wrapper()
    : Memroot_allocator<T>(&m_mem_root)
  {
    init_sql_alloc(PSI_NOT_INSTRUMENTED, &m_mem_root, 1024, 0);
    // memory allocation error is expected, don't abort unit test.
    m_mem_root.error_handler= NULL;
  }

  ~Memroot_allocator_wrapper()
  {
    free_root(&m_mem_root, MYF(0));
  }
};


//
// Test of container with simple objects
//

template<typename T>
class STLAllocTestInt : public ::testing::Test
{
protected:
  T allocator;
};

typedef ::testing::Types<Malloc_allocator_wrapper<int>,
                         Memroot_allocator_wrapper<int> > AllocatorTypesInt;

TYPED_TEST_CASE(STLAllocTestInt, AllocatorTypesInt);


TYPED_TEST(STLAllocTestInt, SimpleVector)
{
  vector<int, TypeParam> v1(this->allocator);
  vector<int, TypeParam> v2(this->allocator);
  for (int i= 0; i < 100; i++)
  {
    v1.push_back(i);
    v2.push_back(100 - i);
  }

  EXPECT_EQ(100U, v1.size());
  EXPECT_EQ(100U, v2.size());

  v1.swap(v2);

  EXPECT_EQ(100U, v1.size());
  EXPECT_EQ(100U, v2.size());

  for (int i= 0; i < 100; i++)
  {
    EXPECT_EQ(i, v2[i]);
    EXPECT_EQ((100 - i), v1[i]);
  }
}


TYPED_TEST(STLAllocTestInt, SimpleList)
{
  list<int, TypeParam> l1(this->allocator);
  list<int, TypeParam> l2(this->allocator);

  for (int i= 0; i < 100; i++)
    l1.push_back(i);

  EXPECT_EQ(100U, l1.size());
  EXPECT_EQ(0U, l2.size());

  l2.splice(l2.begin(), l1);

  EXPECT_EQ(0U, l1.size());
  EXPECT_EQ(100U, l2.size());

  std::reverse(l2.begin(), l2.end());

  for (int i= 0; i < 100; i++)
  {
    EXPECT_EQ((99 - i), l2.front());
    l2.pop_front();
  }

  EXPECT_EQ(0U, l2.size());
}


#ifndef DBUG_OFF
TYPED_TEST(STLAllocTestInt, OutOfMemory)
{
  vector<int, TypeParam> v1(this->allocator);
  v1.reserve(10);
  EXPECT_EQ(10U, v1.capacity());

  DBUG_SET("+d,simulate_out_of_memory");
  ASSERT_THROW(v1.reserve(1000), std::bad_alloc);
}
#endif


//
// Test of container with non-trival objects
//

class Container_object;

template<typename T>
class STLAllocTestObject : public STLAllocTestInt<T>
{ };

typedef ::testing::Types<Malloc_allocator_wrapper<Container_object>,
                         Memroot_allocator_wrapper<Container_object> >
        AllocatorTypesObject;

TYPED_TEST_CASE(STLAllocTestObject, AllocatorTypesObject);

class Container_object
{
  char *buffer;

public:
  Container_object()
  {
    buffer= new char[20];
  }

  Container_object(const Container_object &other)
  {
    buffer= new char[20]; // Don't care about contents
  }

  ~Container_object()
  {
    delete[] buffer;
  }
};

TYPED_TEST(STLAllocTestObject, ContainerObject)
{
  vector<Container_object, TypeParam> v1(this->allocator);
  v1.push_back(Container_object());
  v1.push_back(Container_object());
  v1.push_back(Container_object());
}


//
// Test of container with containers
//

class Container_container;

template<typename T>
class STLAllocTestNested : public STLAllocTestInt<T>
{ };

typedef ::testing::Types<Malloc_allocator_wrapper<Container_container>,
                         Memroot_allocator_wrapper<Container_container> >
         AllocatorTypesNested;

TYPED_TEST_CASE(STLAllocTestNested, AllocatorTypesNested);

class Container_container
{
  deque<Container_object> d;

public:
  Container_container()
  {
    d.push_back(Container_object());
    d.push_back(Container_object());
  }
};

TYPED_TEST(STLAllocTestNested, NestedContainers)
{
  Container_container cc1;
  Container_container cc2;
  list<Container_container, TypeParam> l1(this->allocator);
  l1.push_back(cc1);
  l1.push_back(cc2);
}

} // namespace stlalloc_unittest

#endif // GTEST_HAS_TYPED_TEST)
