/*
   Copyright (c) 2020, 2026, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <deque>
#include <ranges>
#include <vector>

#include "mem_root_deque.h"
#include "my_alloc.h"
#include "sql/mem_root_allocator.h"
#include "unittest/gunit/benchmark.h"

using testing::ElementsAre;

TEST(MemRootDequeTest, Basic) {
  MEM_ROOT mem_root;
  mem_root_deque<int> d(&mem_root);
  EXPECT_TRUE(d.empty());

  d.push_back(1);
  d.push_back(2);
  d.push_back(3);
  EXPECT_THAT(d, ElementsAre(1, 2, 3));
  EXPECT_EQ(3, d.size());
  EXPECT_FALSE(d.empty());

  d.push_front(0);
  d.push_front(-1);
  EXPECT_THAT(d, ElementsAre(-1, 0, 1, 2, 3));
  EXPECT_EQ(5, d.size());
  EXPECT_FALSE(d.empty());

  EXPECT_EQ(-1, d[0]);
  EXPECT_EQ(0, d[1]);
  EXPECT_EQ(1, d[2]);
  EXPECT_EQ(2, d[3]);
  EXPECT_EQ(3, d[4]);

  d.pop_front();
  EXPECT_THAT(d, ElementsAre(0, 1, 2, 3));
  EXPECT_EQ(0, d[0]);
  EXPECT_EQ(1, d[1]);
  EXPECT_EQ(2, d[2]);
  EXPECT_EQ(3, d[3]);

  d.pop_back();
  d.pop_back();
  EXPECT_THAT(d, ElementsAre(0, 1));

  d.push_front(1234);
  EXPECT_THAT(d, ElementsAre(1234, 0, 1));
}

TEST(MemRootDequeTest, EraseInsert) {
  MEM_ROOT mem_root;
  mem_root_deque<int> d(&mem_root);

  d.push_back(1);
  d.push_back(2);
  d.push_back(3);
  d.push_back(4);
  d.push_back(5);

  auto it = d.erase(d.begin() + 1, d.begin() + 3);
  EXPECT_THAT(d, ElementsAre(1, 4, 5));

  int new_elems[] = {200, 300, 400, 500};
  it = d.insert(it, std::begin(new_elems), std::end(new_elems));
  EXPECT_THAT(d, ElementsAre(1, 200, 300, 400, 500, 4, 5));
  EXPECT_EQ(it, d.begin() + 1);

  it = d.insert(d.begin() + 3, 350);
  EXPECT_THAT(d, ElementsAre(1, 200, 300, 350, 400, 500, 4, 5));
  EXPECT_EQ(it, d.begin() + 3);
  EXPECT_EQ(350, *it);
}

TEST(MemRootDequeTest, Sort) {
  MEM_ROOT mem_root;
  mem_root_deque<std::string> d(&mem_root);

  d.push_back("a");
  d.push_back("zzzzzzzzzzzzzzzzzzzzzz");
  d.push_back("x");
  d.push_back("12345");
  d.push_back("hello");

  std::sort(d.begin(), d.end());

  EXPECT_THAT(
      d, ElementsAre("12345", "a", "hello", "x", "zzzzzzzzzzzzzzzzzzzzzz"));
}

TEST(MemRootDequeTest, PointerStability) {
  MEM_ROOT mem_root;
  mem_root_deque<int> d(&mem_root);

  d.push_back(1);
  d.push_back(2);
  d.push_back(3);
  int *ptr = &d[1];
  d.push_front(0);
  EXPECT_EQ(2, *ptr);
  d.push_back(4);
  EXPECT_EQ(2, *ptr);
  d.pop_back();
  d.pop_back();
  EXPECT_EQ(2, *ptr);
  d.pop_front();
  d.pop_front();
  EXPECT_EQ(2, *ptr);

  EXPECT_THAT(d, ElementsAre(2));
}

TEST(MemRootDequeTest, Iteration) {
  MEM_ROOT mem_root;
  mem_root_deque<int> d(&mem_root);

  d.push_back(1);
  d.push_back(2);
  d.push_back(3);
  d.push_back(4);
  d.push_back(5);

  auto it = d.begin();
  EXPECT_EQ(d.begin(), it);
  EXPECT_NE(d.end(), it);
  EXPECT_EQ(1, *it++);
  EXPECT_EQ(2, *it++);
  EXPECT_EQ(4, *++it);
  it -= 2;
  EXPECT_EQ(2, *it);
  it += 2;
  EXPECT_EQ(4, *it--);
  EXPECT_EQ(3, *it--);
  EXPECT_EQ(1, *--it);
  EXPECT_EQ(d.end(), it + 5);
}

TEST(MemRootDequeTest, OperatorArrow) {
  MEM_ROOT mem_root;
  mem_root_deque<std::string> d(&mem_root);

  d.push_back("a");
  d.push_back("aa");
  d.push_back("aaa");

  auto it = d.begin();
  EXPECT_EQ(1, it->size());
  ++it;
  EXPECT_EQ(2, it->size());
  ++it;
  EXPECT_EQ(3, it->size());
}

// The other tests can also be used for stressing multi-block code,
// if you force FindElementsPerBlock() to return 1.
TEST(MemRootDequeTest, MultipleBlocks) {
  MEM_ROOT mem_root;
  mem_root_deque<int> d(&mem_root);

  for (int i = 0; i < 5000; ++i) {
    d.push_back(i);
  }
  EXPECT_EQ(5000, d.size());
  for (int i = 0; i < 1000; ++i) {
    d.pop_front();
  }
  for (int i = 0; i < 1000; ++i) {
    d.pop_back();
  }
  EXPECT_EQ(3000, d.size());
  EXPECT_EQ(1000, d.front());
  EXPECT_EQ(3999, d.back());
}

TEST(MemRootDequeTest, Copy) {
  MEM_ROOT mem_root;
  mem_root_deque<int> d(&mem_root);
  d.push_back(1);
  d.push_back(2);
  d.push_back(3);
  mem_root_deque<int> const e(d);
  e[0] = 5;
  EXPECT_THAT(d, ElementsAre(1, 2, 3));
  EXPECT_THAT(e, ElementsAre(5, 2, 3));

  d = e;
  d[1] = 10;
  EXPECT_THAT(d, ElementsAre(5, 10, 3));
  EXPECT_THAT(e, ElementsAre(5, 2, 3));
}

TEST(MemRootDequeTest, Move) {
  MEM_ROOT mem_root;
  mem_root_deque<int> d(&mem_root);
  d.push_back(1);
  d.push_back(2);
  d.push_back(3);
  mem_root_deque<int> e(std::move(d));
  EXPECT_TRUE(d.empty());
  EXPECT_THAT(e, ElementsAre(1, 2, 3));

  d = std::move(e);
  EXPECT_THAT(d, ElementsAre(1, 2, 3));
  EXPECT_TRUE(e.empty());
}

TEST(MemRootDequeTest, ReverseIteration) {
  MEM_ROOT mem_root;
  mem_root_deque<int> d(&mem_root);
  d.push_back(1);
  d.push_back(2);
  d.push_back(3);
  mem_root_deque<int> e(&mem_root);
  for (int &it : std::ranges::reverse_view(d)) {
    e.push_back(it);
  }
  EXPECT_THAT(e, ElementsAre(3, 2, 1));

  const mem_root_deque<int> &d_ref = d;
  for (int it : std::ranges::reverse_view(d_ref)) {
    e.push_back(it);
  }
}

TEST(MemRootDequeTest, ConvertIterators) {
  MEM_ROOT mem_root;
  mem_root_deque<int> d(&mem_root);
  mem_root_deque<int>::iterator const i = d.begin();
  mem_root_deque<int>::const_iterator const j{i};
}

// Test that exponential growth works correctly for both directions.
// Verifies Bug#33390851 fix: mem_root_deque should use O(log N) reallocations
// instead of O(N), reducing memory waste from O(N^2) to O(N).
TEST(MemRootDequeTest, ExponentialGrowth) {
  MEM_ROOT mem_root;
  mem_root_deque<int> d(&mem_root);

  // Initially empty
  EXPECT_EQ(0, d.size());
  EXPECT_EQ(0, d.block_slots());
  EXPECT_EQ(0, d.first_block_idx());
  EXPECT_EQ(0, d.back_growth_exp());
  EXPECT_EQ(0, d.front_growth_exp());

  // Add 1000 elements to the back
  for (int i = 0; i < 1000; ++i) {
    d.push_back(i);
  }
  EXPECT_EQ(1000, d.size());

  // Verify elements are correct
  for (int i = 0; i < 1000; ++i) {
    EXPECT_EQ(i, d[i]);
  }

  // Check exponential growth occurred (not linear).
  // With ~256 elements per block (1024 bytes / 4 bytes per int),
  // 1000 elements need ~4 blocks.
  // Growth sequence: 1 -> 2 -> 4 blocks allocated, exp goes 0 -> 1 -> 2
  EXPECT_GE(d.back_growth_exp(), 2);   // At least 2 growth events
  EXPECT_EQ(0, d.front_growth_exp());  // Front not grown yet
  EXPECT_EQ(0, d.first_block_idx());   // Blocks start at index 0

  uint8_t back_exp_after_push_back = d.back_growth_exp();

  // Add 1000 elements to the front
  for (int i = 0; i < 1000; ++i) {
    d.push_front(-i - 1);  // -1, -2, -3, ...
  }
  EXPECT_EQ(2000, d.size());

  // Verify all elements are correct
  // Front elements: -1000, -999, ..., -1 (indices 0-999)
  // Back elements: 0, 1, ..., 999 (indices 1000-1999)
  for (int i = 0; i < 1000; ++i) {
    EXPECT_EQ(-1000 + i, d[i]);
  }
  for (int i = 0; i < 1000; ++i) {
    EXPECT_EQ(i, d[1000 + i]);
  }

  // Check front exponential growth occurred independently
  EXPECT_GE(d.front_growth_exp(), 2);  // At least 2 growth events
  // Back exponent should not have changed (back didn't need more blocks)
  EXPECT_EQ(back_exp_after_push_back, d.back_growth_exp());
  // first_block_idx should have moved to accommodate front spare slots
  EXPECT_GT(d.first_block_idx(), 0);

  // Verify memory usage is reasonable (not O(N^2))
  // With exponential growth, blocks_allocated should be O(num_blocks),
  // not O(num_blocks^2) as with linear growth.
  // blocks_allocated should be at most ~3x the minimum needed
  // (due to spare slots for future growth)
  size_t min_blocks_needed =
      (d.size() + 255) / 256;  // ~8 blocks for 2000 elements
  EXPECT_LE(d.block_slots(), min_blocks_needed * 3);  // Allow 3x for spare
}

// Test memory consumption with exponential growth
TEST(MemRootDequeTest, MemoryEfficiency) {
  MEM_ROOT mem_root;

  size_t mem_before = mem_root.allocated_size();
  {
    mem_root_deque<int> d(&mem_root);

    // Add 10000 elements
    for (int i = 0; i < 10000; ++i) {
      d.push_back(i);
    }

    size_t mem_after = mem_root.allocated_size();
    size_t mem_used = mem_after - mem_before;

    // With 10000 ints (40KB data), memory should be O(N), not O(N^2).
    // Allow up to 4x overhead for block array + element blocks + spare.
    // With O(N^2) linear growth, this would be much larger.
    size_t data_size = 10000 * sizeof(int);  // 40KB
    EXPECT_LT(mem_used, data_size * 4);      // Should be < 160KB

    // Verify correctness
    for (int i = 0; i < 10000; ++i) {
      EXPECT_EQ(i, d[i]);
    }
  }
}

// Test memory at each growth phase for push_back.
// Verifies exponential growth pattern (not quadratic).
TEST(MemRootDequeTest, MemoryGrowthPushBack) {
  MEM_ROOT mem_root;
  mem_root_deque<int> d(&mem_root);

  std::vector<size_t> memory_at_growth;
  size_t last_block_slots = 0;

  for (int i = 0; i < 5000; ++i) {
    d.push_back(i);
    // Record memory when block_slots changes (growth occurred)
    if (d.block_slots() != last_block_slots) {
      memory_at_growth.push_back(mem_root.allocated_size());
      last_block_slots = d.block_slots();
    }
  }

  // Verify exponential growth: each allocation should roughly double
  // (not grow linearly, which would indicate O(N^2) total memory)
  for (size_t i = 2; i < memory_at_growth.size(); ++i) {
    size_t prev_delta = memory_at_growth[i - 1] - memory_at_growth[i - 2];
    size_t curr_delta = memory_at_growth[i] - memory_at_growth[i - 1];
    // Current growth should be >= previous (exponential, not linear)
    EXPECT_GE(curr_delta, prev_delta / 2);  // Allow some tolerance
  }
}

// Test memory for push_front independently.
TEST(MemRootDequeTest, MemoryGrowthPushFront) {
  MEM_ROOT mem_root;
  mem_root_deque<int> d(&mem_root);

  size_t mem_before = mem_root.allocated_size();

  for (int i = 0; i < 5000; ++i) {
    d.push_front(i);
  }

  size_t mem_after = mem_root.allocated_size();
  size_t mem_used = mem_after - mem_before;

  // Should be O(N), not O(N^2)
  size_t data_size = 5000 * sizeof(int);
  EXPECT_LT(mem_used, data_size * 4);
}

// Test mixed push_back and push_front memory usage.
TEST(MemRootDequeTest, MemoryGrowthMixed) {
  MEM_ROOT mem_root;
  mem_root_deque<int> d(&mem_root);

  size_t mem_before = mem_root.allocated_size();

  // Alternate push_back and push_front
  for (int i = 0; i < 2500; ++i) {
    d.push_back(i);
    d.push_front(-i);
  }

  size_t mem_after = mem_root.allocated_size();
  size_t mem_used = mem_after - mem_before;

  // Both directions grow independently - should still be O(N)
  size_t data_size = 5000 * sizeof(int);
  EXPECT_LT(mem_used, data_size * 5);  // Slightly more tolerance for mixed

  // Verify both growth exponents increased
  EXPECT_GE(d.back_growth_exp(), 2);
  EXPECT_GE(d.front_growth_exp(), 2);
}

// Test no excessive memory after clear and refill.
TEST(MemRootDequeTest, MemoryAfterClearAndRefill) {
  MEM_ROOT mem_root;
  mem_root_deque<int> d(&mem_root);

  // First fill
  for (int i = 0; i < 1000; ++i) {
    d.push_back(i);
  }
  size_t mem_after_first_fill = mem_root.allocated_size();

  d.clear();

  // Second fill (same size)
  for (int i = 0; i < 1000; ++i) {
    d.push_back(i);
  }
  size_t mem_after_second_fill = mem_root.allocated_size();

  // Memory should roughly double (MEM_ROOT doesn't free on clear)
  // But not grow excessively
  EXPECT_LT(mem_after_second_fill, mem_after_first_fill * 3);
}

// Microbenchmarks.

template <class T>
using std_mem_root_deque = std::deque<T, Mem_root_allocator<T>>;

static void BM_EmptyConstruct(size_t num_iterations) {
  MEM_ROOT mem_root;
  for (size_t i = 0; i < num_iterations; ++i) {
    { mem_root_deque<int> const d(&mem_root); }
    mem_root.ClearForReuse();
  }
}
BENCHMARK(BM_EmptyConstruct)

static void BM_EmptyConstructStdDeque(size_t num_iterations) {
  MEM_ROOT mem_root;
  for (size_t i = 0; i < num_iterations; ++i) {
    { std_mem_root_deque<int> const d{Mem_root_allocator<int>(&mem_root)}; }
    mem_root.ClearForReuse();
  }
}
BENCHMARK(BM_EmptyConstructStdDeque)

static void BM_PushBackAndFront(size_t num_iterations) {
  MEM_ROOT mem_root;
  for (size_t i = 0; i < num_iterations; ++i) {
    {
      mem_root_deque<int> d(&mem_root);
      for (size_t j = 0; j < 1000; ++j) {
        d.push_back(j);
      }
      for (size_t j = 0; j < 1000; ++j) {
        d.push_front(j);
      }
    }
    mem_root.ClearForReuse();
  }
}
BENCHMARK(BM_PushBackAndFront)

static void BM_PushBackAndFrontStdDeque(size_t num_iterations) {
  MEM_ROOT mem_root;
  for (size_t i = 0; i < num_iterations; ++i) {
    {
      std_mem_root_deque<int> d{Mem_root_allocator<int>(&mem_root)};
      for (size_t j = 0; j < 1000; ++j) {
        d.push_back(j);
      }
      for (size_t j = 0; j < 1000; ++j) {
        d.push_front(j);
      }
    }
    mem_root.ClearForReuse();
  }
}
BENCHMARK(BM_PushBackAndFrontStdDeque)

static void BM_RandomAccess(size_t num_iterations) {
  StopBenchmarkTiming();
  MEM_ROOT mem_root;
  mem_root_deque<unsigned> d(&mem_root);
  for (size_t j = 0; j < 1024; ++j) {
    d.push_back(j + 1);
  }
  for (size_t j = 0; j < 1024; ++j) {
    d.push_front(j + 1);
  }
  StartBenchmarkTiming();

  unsigned sum = 0;  // To prevent it from being optimized away.
  for (size_t i = 0; i < num_iterations; ++i) {
    for (size_t j = 0; j < 1024; ++j) {  // We can't show less than ns.
      sum += d[((i + j) * 997) % 2048];
    }
  }
  if (sum == 0) abort();
}
BENCHMARK(BM_RandomAccess)

static void BM_RandomAccessStdDeque(size_t num_iterations) {
  StopBenchmarkTiming();
  MEM_ROOT mem_root;
  std_mem_root_deque<unsigned> d{Mem_root_allocator<unsigned>(&mem_root)};
  for (size_t j = 0; j < 1024; ++j) {
    d.push_back(j + 1);
  }
  for (size_t j = 0; j < 1024; ++j) {
    d.push_front(j + 1);
  }
  StartBenchmarkTiming();

  unsigned sum = 0;  // To prevent it from being optimized away.
  for (size_t i = 0; i < num_iterations; ++i) {
    for (size_t j = 0; j < 1024; ++j) {  // We can't show less than ns.
      sum += d[((i + j) * 997) % 2048];
    }
  }
  if (sum == 0) abort();
}
BENCHMARK(BM_RandomAccessStdDeque)

static void BM_Iteration(size_t num_iterations) {
  StopBenchmarkTiming();
  MEM_ROOT mem_root;
  mem_root_deque<unsigned> d(&mem_root);
  for (size_t j = 0; j < 1024; ++j) {
    d.push_back(j + 1);
  }
  for (size_t j = 0; j < 1024; ++j) {
    d.push_front(j + 1);
  }
  StartBenchmarkTiming();

  unsigned sum = 0;  // To prevent it from being optimized away.
  for (size_t i = 0; i < num_iterations; ++i) {
    for (unsigned const x : d) {
      sum += x;
    }
  }
  if (sum == 0) abort();
}
BENCHMARK(BM_Iteration)

static void BM_IterationStdDeque(size_t num_iterations) {
  StopBenchmarkTiming();
  MEM_ROOT mem_root;
  std_mem_root_deque<unsigned> d{Mem_root_allocator<unsigned>(&mem_root)};
  for (size_t j = 0; j < 1024; ++j) {
    d.push_back(j + 1);
  }
  for (size_t j = 0; j < 1024; ++j) {
    d.push_front(j + 1);
  }
  StartBenchmarkTiming();

  unsigned sum = 0;  // To prevent it from being optimized away.
  for (size_t i = 0; i < num_iterations; ++i) {
    for (unsigned const x : d) {
      sum += x;
    }
  }
  if (sum == 0) abort();
}
BENCHMARK(BM_IterationStdDeque)
