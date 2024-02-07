/*
   Copyright (c) 2015, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include <gtest/gtest.h>
#include <stddef.h>
#include <sys/types.h>
#include <memory>
#include <string>

#include "my_alloc.h"
#include "my_inttypes.h"
#include "my_sys.h"
#include "mysys_err.h"

extern "C" void mock_error_handler_hook(uint err, const char *str, myf MyFlags);

/**
  An alternative error_handler for non-server unit tests since it does
  not rely on THD.  It sets the global error handler function.
*/
class Mock_global_error_handler {
 public:
  Mock_global_error_handler(uint expected_error)
      : m_expected_error(expected_error), m_handle_called(0) {
    current = this;
    m_old_error_handler_hook = error_handler_hook;
    error_handler_hook = mock_error_handler_hook;
  }

  virtual ~Mock_global_error_handler() {
    if (m_expected_error == 0) {
      EXPECT_EQ(0, m_handle_called);
    } else {
      EXPECT_GT(m_handle_called, 0);
    }
    error_handler_hook = m_old_error_handler_hook;
    current = nullptr;
  }

  void error_handler(uint err) {
    EXPECT_EQ(m_expected_error, err);
    ++m_handle_called;
  }

  int handle_called() const { return m_handle_called; }

  static Mock_global_error_handler *current;

 private:
  uint m_expected_error;
  int m_handle_called;

  ErrorHandlerFunctionPointer m_old_error_handler_hook;
};

Mock_global_error_handler *Mock_global_error_handler::current = nullptr;

/*
  Error handler function.
*/
extern "C" void mock_error_handler_hook(uint err, const char *, myf) {
  if (Mock_global_error_handler::current)
    Mock_global_error_handler::current->error_handler(err);
}

namespace my_alloc_unittest {

const size_t num_iterations = 1ULL;

class MyAllocTest : public ::testing::TestWithParam<size_t> {
 protected:
  void SetUp() override {
    ::new ((void *)&m_root) MEM_ROOT(PSI_NOT_INSTRUMENTED, 1024);
  }
  void TearDown() override { m_root.Clear(); }
  size_t m_num_objects;
  MEM_ROOT m_root;
};

class MyPreAllocTest : public ::testing::Test {
 protected:
  void SetUp() override {
    ::new ((void *)&m_prealloc_root) MEM_ROOT(PSI_NOT_INSTRUMENTED, 1024);
  }
  void TearDown() override { m_prealloc_root.Clear(); }
  size_t m_num_objects;
  MEM_ROOT m_prealloc_root;
};

size_t test_values[] = {100, 1000, 10000, 100000};

INSTANTIATE_TEST_SUITE_P(MyAlloc, MyAllocTest,
                         ::testing::ValuesIn(test_values));

TEST_P(MyAllocTest, NoMemoryLimit) {
  m_num_objects = GetParam();
  for (size_t ix = 0; ix < num_iterations; ++ix) {
    for (size_t objcount = 0; objcount < m_num_objects; ++objcount)
      EXPECT_NE(nullptr, m_root.Alloc(8));
  }
  // Normally larger, but with Valgrind/ASan, we'll get exact-sized blocks,
  // so also allow equal.
  EXPECT_GE(m_root.allocated_size(), num_iterations * m_num_objects * 8);
}

TEST_P(MyAllocTest, WithMemoryLimit) {
  m_num_objects = GetParam();
  m_root.set_max_capacity(num_iterations * m_num_objects * 8);
  for (size_t ix = 0; ix < num_iterations; ++ix) {
    for (size_t objcount = 0; objcount < m_num_objects; ++objcount)
      EXPECT_NE(nullptr, m_root.Alloc(8));
  }
  EXPECT_EQ(m_root.allocated_size(), num_iterations * m_num_objects * 8);
}

TEST_F(MyAllocTest, CheckErrorReporting) {
  EXPECT_NE(nullptr, m_root.Alloc(1000));
  m_root.set_max_capacity(100);
  EXPECT_EQ(nullptr, m_root.Alloc(1000));
  m_root.set_error_for_capacity_exceeded(true);
  Mock_global_error_handler error_handler(EE_CAPACITY_EXCEEDED);
  EXPECT_NE(nullptr, m_root.Alloc(1000));
  EXPECT_EQ(1, error_handler.handle_called());

  EXPECT_FALSE(m_root.ForceNewBlock(2048));
  EXPECT_EQ(2, error_handler.handle_called());
}

TEST_F(MyAllocTest, MoveConstructorDoesNotLeak) {
  MEM_ROOT alloc1(PSI_NOT_INSTRUMENTED, 512);
  (void)alloc1.Alloc(10);
  MEM_ROOT alloc2(PSI_NOT_INSTRUMENTED, 512);
  (void)alloc2.Alloc(30);
  alloc1 = std::move(alloc2);
}

TEST_F(MyAllocTest, ExceptionalBlocksAreNotReusedForLargerAllocations) {
  MEM_ROOT alloc(PSI_NOT_INSTRUMENTED, 512);
  void *ptr = alloc.Alloc(600);
  alloc.ClearForReuse();

  if (alloc.allocated_size() == 0) {
    /*
      The MEM_ROOT was all cleared out (probably because we're running under
      Valgrind/ASAN), so we are obviously not doing any reuse. Moreover,
      the test below is unsafe in this case, since the system malloc() could
      reuse the block.
    */
    return;
  }

  // The allocated block is too small to satisfy this new, larger allocation.
  void *ptr2 = alloc.Alloc(605);
  EXPECT_NE(ptr, ptr2);
}

TEST_F(MyAllocTest, RawInterface) {
  MEM_ROOT alloc(PSI_NOT_INSTRUMENTED, 512);

  // Nothing allocated yet.
  std::pair<char *, char *> block = alloc.Peek();
  EXPECT_EQ(0, block.second - block.first);

  // Create a block.
  alloc.ForceNewBlock(16);
  block = alloc.Peek();
  EXPECT_EQ(512, block.second - block.first);

  // Write and commit some memory.
  char *store_ptr = reinterpret_cast<char *>(block.first);
  strcpy(store_ptr, "12345");
  alloc.RawCommit(6);
  block = alloc.Peek();
  EXPECT_EQ(506, block.second - block.first);

  // Get a new block.
  alloc.ForceNewBlock(512);
  block = alloc.Peek();
#if defined(HAVE_VALGRIND) || defined(HAVE_ASAN)
  EXPECT_EQ(512, block.second - block.first);
#else
  EXPECT_EQ(768, block.second - block.first);
#endif

  // The value should still be there.
  EXPECT_STREQ("12345", store_ptr);

  // Get a new block to satisfy more than the current block size (512 * 1.5^2).
  EXPECT_FALSE(alloc.ForceNewBlock(2048));
  block = alloc.Peek();
  EXPECT_EQ(2048, block.second - block.first);
}

TEST_F(MyAllocTest, ArrayAllocInitialization) {
  MEM_ROOT alloc(PSI_NOT_INSTRUMENTED, 512);

  // No default value means each element is value-initialized. For int, it means
  // they are set to 0.
  int *int_array1 = alloc.ArrayAlloc<int>(100);
  ASSERT_NE(nullptr, int_array1);
  for (int i = 0; i < 100; ++i) {
    EXPECT_EQ(0, int_array1[i]);
  }

  // Initialize to explicit value.
  int *int_array2 = alloc.ArrayAlloc<int>(100, 123);
  ASSERT_NE(nullptr, int_array2);
  for (int i = 0; i < 100; ++i) {
    EXPECT_EQ(123, int_array2[i]);
  }

  // Initialize from rvalue. (Verifies that a bug, which made it only initialize
  // the first element correctly, is fixed.)
  std::string *string_array1 = alloc.ArrayAlloc<std::string>(
      10, std::string("abcdefghijklmnopqrstuvwxyz"));
  ASSERT_NE(nullptr, string_array1);
  for (int i = 0; i < 10; ++i) {
    EXPECT_EQ("abcdefghijklmnopqrstuvwxyz", string_array1[i]);
  }
  std::destroy_n(string_array1, 10);

  // Should be allowed to create an array of a class which is not
  // copy-constructible.
  auto uptr_array1 = alloc.ArrayAlloc<std::unique_ptr<int>>(10);
  ASSERT_NE(nullptr, uptr_array1);
  auto uptr_array2 = alloc.ArrayAlloc<std::unique_ptr<int>>(10, nullptr);
  ASSERT_NE(nullptr, uptr_array2);
  for (int i = 0; i < 10; ++i) {
    EXPECT_EQ(nullptr, uptr_array1[i]);
    EXPECT_EQ(nullptr, uptr_array2[i]);
  }

  // This should give a compiler error, because it attempts to copy a value of a
  // non-copyable type:
  //
  // alloc.ArrayAlloc<std::unique_ptr<int>>(3, std::make_unique<int>(123));
}

}  // namespace my_alloc_unittest
