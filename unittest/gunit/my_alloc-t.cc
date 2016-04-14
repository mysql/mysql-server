/*
   Copyright (c) 2015, 2016, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   51 Franklin Street, Suite 500, Boston, MA 02110-1301 USA */

// First include (the generated) my_config.h, to get correct platform defines.
#include "my_config.h"
#include <gtest/gtest.h>
#include "mysys_err.h"

#include "my_sys.h"

extern "C" void mock_error_handler_hook(uint err, const char *str, myf MyFlags);

/**
  An alternative error_handler for non-server unit tests since it does
  not rely on THD.  It sets the global error handler function.
*/
class Mock_global_error_handler
{
public:
  Mock_global_error_handler(uint expected_error):
    m_expected_error(expected_error),
    m_handle_called(0)
  {
    current= this;
    m_old_error_handler_hook= error_handler_hook;
    error_handler_hook = mock_error_handler_hook;
  }

  virtual ~Mock_global_error_handler()
  {
    if (m_expected_error == 0)
    {
      EXPECT_EQ(0, m_handle_called);
    }
    else
    {
      EXPECT_GT(m_handle_called, 0);
    }
    error_handler_hook= m_old_error_handler_hook;
    current= NULL;
  }

  void error_handler(uint err)
  {
    EXPECT_EQ(m_expected_error, err);
    ++m_handle_called;
  }

  int handle_called() const { return m_handle_called; }

  static Mock_global_error_handler *current;

private:
  uint m_expected_error;
  int  m_handle_called;

  void (*m_old_error_handler_hook)(uint, const char *, myf);
};

Mock_global_error_handler *Mock_global_error_handler::current= NULL;

/*
  Error handler function.
*/
extern "C" void mock_error_handler_hook(uint err, const char *str, myf MyFlags)
{
  if (Mock_global_error_handler::current)
    Mock_global_error_handler::current->error_handler(err);
}

namespace my_alloc_unittest {

const size_t num_iterations= 1ULL;

class MyAllocTest : public ::testing::TestWithParam<size_t>
{
protected:
  virtual void SetUp()
  {
    init_alloc_root(PSI_NOT_INSTRUMENTED, &m_root, 1024, 0);
  }
  virtual void TearDown()
  {
    free_root(&m_root, MYF(0));
  }
  size_t   m_num_objects;
  MEM_ROOT m_root;
};

class MyPreAllocTest : public ::testing::Test
{
protected:
  virtual void SetUp()
  {
    init_alloc_root(PSI_NOT_INSTRUMENTED, &m_prealloc_root, 1024, 2048);
  }
  virtual void TearDown()
  {
    free_root(&m_prealloc_root, MYF(0));
  }
  size_t   m_num_objects;
  MEM_ROOT m_prealloc_root;
};



size_t test_values[]= {100, 1000, 10000, 100000 };

INSTANTIATE_TEST_CASE_P(MyAlloc, MyAllocTest,
                        ::testing::ValuesIn(test_values));

TEST_P(MyAllocTest, NoMemoryLimit)
{
  m_num_objects= GetParam();
  for (size_t ix= 0; ix < num_iterations; ++ix)
  {
    for (size_t objcount= 0; objcount < m_num_objects; ++objcount)
      alloc_root(&m_root, 100);
  }
}

TEST_P(MyAllocTest, WithMemoryLimit)
{
  m_num_objects= GetParam();
  set_memroot_max_capacity(&m_root, num_iterations * m_num_objects * 100);
  for (size_t ix= 0; ix < num_iterations; ++ix)
  {
    for (size_t objcount= 0; objcount < m_num_objects; ++objcount)
      alloc_root(&m_root, 100);
  }
}

TEST_F(MyAllocTest, CheckErrorReporting)
{
  const void *null_pointer= NULL;
  EXPECT_TRUE(alloc_root(&m_root, 1000));
  set_memroot_max_capacity(&m_root, 100);
  EXPECT_EQ(null_pointer, alloc_root(&m_root, 1000));
  set_memroot_error_reporting(&m_root, true);
  Mock_global_error_handler error_handler(EE_CAPACITY_EXCEEDED);
  EXPECT_TRUE(alloc_root(&m_root, 1000));
  EXPECT_EQ(1, error_handler.handle_called());
}

TEST_F(MyPreAllocTest, PreAlloc)
{
  // PREALLOCATE_MEMORY_CHUNKS is not defined for valgrind and ASAN
#if !defined(HAVE_VALGRIND) && !defined(HAVE_ASAN)
  const void *null_pointer= NULL;
  // MEMROOT has pre-allocated 2048 bytes memory plus some overhead
  size_t pre_allocated= m_prealloc_root.allocated_size;
  EXPECT_LT((unsigned int)2048, pre_allocated);

  // This will eat of pre-allocated memory, no more should be allocated
  EXPECT_TRUE(alloc_root(&m_prealloc_root, 1000));
  EXPECT_EQ(pre_allocated, m_prealloc_root.allocated_size);

  set_memroot_max_capacity(&m_prealloc_root, 100);
  // Sufficient memory has been pre-allocated, so first alloc below will succeed
  EXPECT_TRUE(alloc_root(&m_prealloc_root, 1000));
  EXPECT_EQ(null_pointer, alloc_root(&m_prealloc_root, 100));
  EXPECT_EQ(pre_allocated, m_prealloc_root.allocated_size);

  // Setting error reporting. Error is flagged but allocation succeeds
  set_memroot_error_reporting(&m_prealloc_root, true);
  {
    Mock_global_error_handler error_handler(EE_CAPACITY_EXCEEDED);
    EXPECT_TRUE(alloc_root(&m_prealloc_root, 1000));
    EXPECT_EQ(1, error_handler.handle_called());
    EXPECT_LT(pre_allocated, m_prealloc_root.allocated_size);
    pre_allocated= m_prealloc_root.allocated_size;
  }
  set_memroot_error_reporting(&m_prealloc_root, false);
  
  //This will just mark the blocks free.
  free_root(&m_prealloc_root, MY_MARK_BLOCKS_FREE);
  EXPECT_EQ(pre_allocated, m_prealloc_root.allocated_size);

  set_memroot_max_capacity(&m_prealloc_root, 2048);
  reset_root_defaults(&m_prealloc_root, 1024, 0);
  EXPECT_EQ(pre_allocated, m_prealloc_root.allocated_size);
  reset_root_defaults(&m_prealloc_root, 1024, 1024);
  EXPECT_LT((unsigned int)1024, m_prealloc_root.allocated_size);

  reset_root_defaults(&m_prealloc_root, 512, 1024);
  EXPECT_LT((unsigned int)1024, m_prealloc_root.allocated_size);
  pre_allocated= m_prealloc_root.allocated_size;
  // This allocation will use pre-alocated memory
  EXPECT_TRUE(alloc_root(&m_prealloc_root, 1024));
  EXPECT_EQ(pre_allocated, m_prealloc_root.allocated_size);
  // Will allocate more memory
  EXPECT_TRUE(alloc_root(&m_prealloc_root, 512));
  EXPECT_LT((unsigned int)1526, m_prealloc_root.allocated_size);
  pre_allocated= m_prealloc_root.allocated_size;
  //  This will not succeed
  EXPECT_EQ(null_pointer, alloc_root(&m_prealloc_root, 512));

  free_root(&m_prealloc_root, MY_KEEP_PREALLOC);
  EXPECT_LT((unsigned int)1024, m_prealloc_root.allocated_size);

  // Specified pre_alloc_size is above capacity. Expect no pre-allocation
  reset_root_defaults(&m_prealloc_root, 512, 4096);
  EXPECT_EQ((unsigned int)0, m_prealloc_root.allocated_size);

  free_root(&m_prealloc_root, 0);
  EXPECT_EQ((unsigned int)0, m_prealloc_root.allocated_size);
#endif
}

}
