/* Copyright (C) 2009 Sun Microsystems, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

/*
  This is a simple example of how to use the google unit test framework.

  For an introduction to the constructs used below, see:
  http://code.google.com/p/googletest/wiki/GoogleTestPrimer
*/

// First include (the generated) my_config.h, to get correct platform defines,
// then gtest.h (before any other MySQL headers), to avoid min() macros etc ...
#include "my_config.h"
#include <gtest/gtest.h>

#include "sql_list.h"

#include "thr_malloc.h"
#include "sql_string.h"
#include "sql_error.h"
#include <my_pthread.h>

pthread_key(MEM_ROOT**, THR_MALLOC);
pthread_key(THD*, THR_THD);

extern "C" void sql_alloc_error_handler(void)
{
  ADD_FAILURE();
}

namespace {

// A simple helper function to determine array size.
template <class T, int size>
int array_size(const T (&)[size])
{
  return size;
}

// A simple helper function to insert values into a List.
template <class T, int size>
void insert_values(T (&array)[size], List<T> *list)
{
  for (int ix= 0; ix < size; ++ix)
  {
    EXPECT_FALSE(list->push_back(&array[ix]));
  }
}

/*
  The fixture for testing the MySQL List and List_iterator classes.
  A fresh instance of this class will be created for each of the
  TEST_F functions below.
  The functions SetUp(), TearDown(), SetUpTestCase(), TearDownTestCase() are
  inherited from ::testing::Test (google naming style differs from MySQL).
*/
class Sql_list_test : public ::testing::Test
{
protected:
  Sql_list_test()
    : m_mem_root_p(&m_mem_root), m_int_list(), m_int_list_iter(m_int_list)
  {
  }

  virtual void SetUp()
  {
    init_sql_alloc(&m_mem_root, 1024, 0);
    ASSERT_EQ(0, my_pthread_setspecific_ptr(THR_MALLOC, &m_mem_root_p));
    MEM_ROOT *root= *my_pthread_getspecific_ptr(MEM_ROOT**, THR_MALLOC);
    ASSERT_EQ(root, m_mem_root_p);
  }

  virtual void TearDown()
  {
    free_root(&m_mem_root, MYF(0));
  }

  static void SetUpTestCase()
  {
    ASSERT_EQ(0, pthread_key_create(&THR_THD, NULL));
    ASSERT_EQ(0, pthread_key_create(&THR_MALLOC, NULL));
  }

  static void TearDownTestCase()
  {
    pthread_key_delete(THR_THD);
    pthread_key_delete(THR_MALLOC);
  }

  MEM_ROOT m_mem_root;
  MEM_ROOT *m_mem_root_p;
  List<int> m_int_list;
  List_iterator<int> m_int_list_iter;

private:
  // Declares (but does not define) copy constructor and assignment operator.
  GTEST_DISALLOW_COPY_AND_ASSIGN_(Sql_list_test);
};


// Tests that we can construct and destruct lists.
TEST_F(Sql_list_test, construct_and_destruct)
{
  EXPECT_TRUE(m_int_list.is_empty());
  List<int> *p_int_list= new List<int>;
  EXPECT_TRUE(p_int_list->is_empty());
  delete p_int_list;
}


// Tests basic operations push and pop.
TEST_F(Sql_list_test, basic_operations)
{
  int i1= 1;
  int i2= 2;
  EXPECT_FALSE(m_int_list.push_front(&i1));
  EXPECT_FALSE(m_int_list.push_back(&i2));
  EXPECT_FALSE(m_int_list.is_empty());
  EXPECT_EQ(2U, m_int_list.elements);

  EXPECT_EQ(&i1, m_int_list.head());
  EXPECT_EQ(&i1, m_int_list.pop());
  EXPECT_EQ(&i2, m_int_list.head());
  EXPECT_EQ(&i2, m_int_list.pop());
  EXPECT_TRUE(m_int_list.is_empty()) << "The list should be empty now!";
}


// Tests list copying.
TEST_F(Sql_list_test, deep_copy)
{
  int values[] = {11, 22, 33, 42, 5};
  insert_values(values, &m_int_list);
  MEM_ROOT *mem_root= (MEM_ROOT*) malloc(1 << 20);
  init_alloc_root(mem_root, 4096, 4096);
  List<int> list_copy(m_int_list, mem_root);
  EXPECT_EQ(list_copy.elements, m_int_list.elements);
  while (!list_copy.is_empty())
  {
    EXPECT_EQ(*m_int_list.pop(), *list_copy.pop());
  }
  EXPECT_TRUE(m_int_list.is_empty());
  free(mem_root);
}


// Tests that we can iterate over values.
TEST_F(Sql_list_test, iterate)
{
  int values[] = {3, 2, 1};
  insert_values(values, &m_int_list);
  for (int ix= 0; ix < array_size(values); ++ix)
  {
    EXPECT_EQ(values[ix], *m_int_list_iter++);
  }
  m_int_list_iter.init(m_int_list);
  int *value;
  int value_number= 0;
  while ((value= m_int_list_iter++))
  {
    EXPECT_EQ(values[value_number++], *value);
  }
}


// A simple helper class for testing intrusive lists.
class Linked_node : public ilink
{
public:
  Linked_node(int val) : m_value(val) {}
  int get_value() const { return m_value; }
private:
  int m_value;
};


// An example of a test without any fixture.
TEST(Sql_ilist_test, construct_and_destruct)
{
  I_List<Linked_node> i_list;
  I_List_iterator<Linked_node> i_list_iter(i_list);
  EXPECT_TRUE(i_list.is_empty());
  const Linked_node *null_node= NULL;
  EXPECT_EQ(null_node, i_list_iter++);
}


// Tests iteration over intrusive lists.
TEST(Sql_ilist_test, iterate)
{
  I_List<Linked_node> i_list;
  I_List_iterator<Linked_node> i_list_iter(i_list);
  int values[] = {11, 22, 33, 42, 5};
  for (int ix= 0; ix < array_size(values); ++ix)
  {
    i_list.push_back(new Linked_node(values[ix]));
  }

  Linked_node *node;
  int value_number= 0;
  while ((node= i_list_iter++))
  {
    EXPECT_EQ(values[value_number++], node->get_value());
  }
}

}  // namespace
