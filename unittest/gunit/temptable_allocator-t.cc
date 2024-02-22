/* Copyright (c) 2016, 2024, Oracle and/or its affiliates.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2.0, as published by the
Free Software Foundation.

This program is designed to work with certain software (including
but not limited to OpenSSL) that is licensed under separate terms,
as designated in a particular file or component or in included license
documentation.  The authors of MySQL hereby grant you an additional
permission to link the program and your derivative works with the
separately licensed software that they have either included with
the program or referenced in the documentation.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License, version 2.0,
for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include <gtest/gtest.h>
#include <array>

#include "storage/temptable/include/temptable/allocator.h"
#include "storage/temptable/include/temptable/block.h"
#include "storage/temptable/include/temptable/constants.h"

namespace temptable_allocator_unittest {

// Needed for making it possible to use user-defined literals (e.g. 1_MiB) when
// instantiating (generating) test-cases below
using temptable::operator"" _KiB;
using temptable::operator"" _MiB;

/** GoogleTest macros for testing exceptions (EXPECT_THROW, EXPECT_NO_THROW,
 * EXPECT_ANY_THROW, etc.) do not provide direct means to inspect the value of
 * exception that has been thrown.
 *
 * Indirectly, it is possible to inspect the value but the test-body becomes
 * unnecessarily verbose, longer and more prone to introducing errors. E.g.
 *
 *    try {
 *        Foo foo;
 *        foo.bar();
 *        FAIL() << "We must not reach here. Expected std::out_of_range";
 *    }
 *    catch (const std::out_of_range& ex) {
 *        EXPECT_EQ(ex.what(), std::string("Out of range"));
 *    }
 *    catch (...) {
 *        FAIL() << "We must not reach here. Expected std::out_of_range";
 *    }
 *
 * Following set of macros extend GoogleTest set of macros and enables
 * us to express our intent in much more cleaner way. E.g.
 *
 *   Foo foo;
 *   EXPECT_THROW_WITH_VALUE_STR(foo.bar(), std::out_of_range, "Out of range");
 *
 * EXPECT_THROW_WITH_VALUE macro is for catching exceptions which are not
 * derived from std::exception and whose value can be inspected by a mere usage
 * of operator==.
 *
 * EXPECT_THROW_WITH_VALUE_STR macro is for catching exceptions which provide
 * ::what() interface to inspect the value, such as all exceptions which
 * are derived from std::exception.
 * */
#define EXPECT_THROW_WITH_VALUE(stmt, etype, value) \
  EXPECT_THROW(                                     \
      try { stmt; } catch (const etype &ex) {       \
        EXPECT_EQ(ex, value);                       \
        throw;                                      \
      },                                            \
      etype)
#define EXPECT_THROW_WITH_VALUE_STR(stmt, etype, str) \
  EXPECT_THROW(                                       \
      try { stmt; } catch (const etype &ex) {         \
        EXPECT_EQ(std::string(ex.what()), str);       \
        throw;                                        \
      },                                              \
      etype)

// A "probe" which gains us read-only access to temptable::MemoryMonitor.
// Necessary for implementing certain unit-tests.
struct MemoryMonitorReadOnlyProbe : public temptable::MemoryMonitor {
  static size_t ram_consumption() {
    return temptable::MemoryMonitor::RAM::consumption();
  }
  static size_t ram_threshold() {
    return temptable::MemoryMonitor::RAM::threshold();
  }
  static bool mmap_enabled() { return temptable_use_mmap; }
  static size_t mmap_consumption() {
    return temptable::MemoryMonitor::MMAP::consumption();
  }
  static size_t mmap_threshold() {
    return temptable::MemoryMonitor::MMAP::threshold();
  }
};

// A "probe" which enables us to hijack the temptable::MemoryMonitor.
// Necessary for implementing certain unit-tests.
struct MemoryMonitorHijackProbe : public MemoryMonitorReadOnlyProbe {
  static size_t ram_consumption_reset() {
    auto current_consumption = temptable::MemoryMonitor::RAM::consumption();
    return temptable::MemoryMonitor::RAM::decrease(current_consumption);
  }
  static size_t ram_consumption_set(size_t consumption) {
    MemoryMonitorHijackProbe::ram_consumption_reset();
    return temptable::MemoryMonitor::RAM::increase(consumption);
  }
  static size_t mmap_consumption_reset() {
    auto current_consumption = temptable::MemoryMonitor::MMAP::consumption();
    return temptable::MemoryMonitor::MMAP::decrease(current_consumption);
  }
  static void mmap_enable() { temptable_use_mmap = true; }
  static void mmap_disable() { temptable_use_mmap = false; }
  static void max_ram_set(size_t new_max_ram) {
    temptable_max_ram = new_max_ram;
  }
  static void max_mmap_set(size_t new_max_mmap) {
    temptable_max_mmap = new_max_mmap;
  }
};

class TempTableAllocator : public ::testing::Test {
 protected:
  void SetUp() override {
    // Enable MMAP by default. We need to set it first, so the mmap_threshold()
    // is not zero.
    MemoryMonitorHijackProbe::mmap_enable();

    // Store the default thresholds of RAM and MMAP so we can restore them to
    // the original values prior to starting a new test
    m_default_ram_threshold = MemoryMonitorHijackProbe::ram_threshold();
    m_default_mmap_threshold = MemoryMonitorHijackProbe::mmap_threshold();

    // Reset the RAM and MMAP consumption counters to zero
    EXPECT_EQ(MemoryMonitorHijackProbe::ram_consumption_reset(), 0);
    EXPECT_EQ(MemoryMonitorHijackProbe::mmap_consumption_reset(), 0);
  }

  void TearDown() override {
    // Check all memory was released.
    EXPECT_EQ(MemoryMonitorHijackProbe::ram_consumption(), 0);
    EXPECT_EQ(MemoryMonitorHijackProbe::mmap_consumption(), 0);

    // Restore the original RAM and MMAP thresholds
    MemoryMonitorHijackProbe::max_ram_set(m_default_ram_threshold);
    MemoryMonitorHijackProbe::max_mmap_set(m_default_mmap_threshold);
  }

  size_t m_default_ram_threshold;
  size_t m_default_mmap_threshold;
};

TEST_F(TempTableAllocator, basic) {
  temptable::TableResourceMonitor table_resource_monitor(16 * 1024 * 1024);
  temptable::Block shared_block;
  EXPECT_TRUE(shared_block.is_empty());
  temptable::Allocator<uint8_t> allocator(&shared_block,
                                          table_resource_monitor);

  constexpr size_t n_allocate = 128;
  std::array<uint8_t *, n_allocate> a;
  constexpr size_t n_elements = 16;

  for (size_t i = 0; i < n_allocate; ++i) {
    a[i] = allocator.allocate(n_elements);

    for (size_t j = 0; j < n_elements; ++j) {
      a[i][j] = 0xB;
    }
  }

  EXPECT_FALSE(shared_block.is_empty());

  for (size_t i = 0; i < n_allocate; ++i) {
    allocator.deallocate(a[i], n_elements);
  }

  // Physically deallocate the shared-block (allocator keeps it alive
  // intentionally)
  EXPECT_FALSE(shared_block.is_empty());
  temptable::Prefer_RAM_over_MMAP_policy::block_freed(shared_block.size(),
                                                      shared_block.type());
  shared_block.destroy();
  EXPECT_TRUE(shared_block.is_empty());
}

TEST_F(TempTableAllocator,
       allocation_successful_when_shared_block_is_not_available) {
  temptable::TableResourceMonitor table_resource_monitor(16 * 1024 * 1024);
  // No shared-block is available to be used by the allocator
  temptable::Allocator<uint8_t> allocator(nullptr, table_resource_monitor);
  uint32_t n_elements = 16;

  // Trigger the allocation
  uint8_t *chunk = nullptr;
  EXPECT_NO_THROW(chunk = allocator.allocate(n_elements));
  EXPECT_NE(chunk, nullptr);

  // Clean-up
  allocator.deallocate(chunk, n_elements);
}

TEST_F(TempTableAllocator, shared_block_is_kept_after_last_deallocation) {
  temptable::TableResourceMonitor table_resource_monitor(16 * 1024 * 1024);
  temptable::Block shared_block;
  EXPECT_TRUE(shared_block.is_empty());
  temptable::Allocator<uint8_t> allocator(&shared_block,
                                          table_resource_monitor);

  uint8_t *ptr = allocator.allocate(16);
  EXPECT_FALSE(shared_block.is_empty());

  allocator.deallocate(ptr, 16);

  // Physically deallocate the shared-block (allocator keeps it alive
  // intentionally)
  EXPECT_FALSE(shared_block.is_empty());
  temptable::Prefer_RAM_over_MMAP_policy::block_freed(shared_block.size(),
                                                      shared_block.type());
  shared_block.destroy();
  EXPECT_TRUE(shared_block.is_empty());
}

TEST_F(TempTableAllocator, rightmost_chunk_deallocated_reused_for_allocation) {
  temptable::TableResourceMonitor table_resource_monitor(16 * 1024 * 1024);
  temptable::Block shared_block;
  EXPECT_TRUE(shared_block.is_empty());
  temptable::Allocator<uint8_t> allocator(&shared_block,
                                          table_resource_monitor);

  // Allocate first Chunk which is less than the 1MB
  size_t first_chunk_size = 512 * 1024;
  uint8_t *first_chunk = allocator.allocate(first_chunk_size);

  // Calculate and allocate second chunk in such a way that
  // it lies within the block and fills it
  size_t first_chunk_actual_size =
      temptable::Chunk::size_hint(first_chunk_size);
  size_t space_left_in_block =
      shared_block.size() -
      temptable::Block::size_hint(first_chunk_actual_size);
  size_t second_chunk_size =
      space_left_in_block - (first_chunk_actual_size - first_chunk_size);
  uint8_t *second_chunk = allocator.allocate(second_chunk_size);

  // Make sure that pointers (Chunk's) are from same blocks
  EXPECT_EQ(temptable::Block(temptable::Chunk(first_chunk)),
            temptable::Block(temptable::Chunk(second_chunk)));

  EXPECT_FALSE(shared_block.can_accommodate(1));

  // Deallocate Second Chunk
  allocator.deallocate(second_chunk, second_chunk_size);

  // Allocate Second Chunk again
  second_chunk = allocator.allocate(second_chunk_size);

  // Make sure that pointers (Chunk's) are from same blocks
  EXPECT_EQ(temptable::Block(temptable::Chunk(first_chunk)),
            temptable::Block(temptable::Chunk(second_chunk)));

  // Deallocate Second Chunk
  allocator.deallocate(second_chunk, second_chunk_size);

  // Deallocate First Chunk
  allocator.deallocate(first_chunk, first_chunk_size);

  // Physically deallocate the shared-block (allocator keeps it alive
  // intentionally)
  EXPECT_FALSE(shared_block.is_empty());
  temptable::Prefer_RAM_over_MMAP_policy::block_freed(shared_block.size(),
                                                      shared_block.type());
  shared_block.destroy();
  EXPECT_TRUE(shared_block.is_empty());
}

TEST_F(TempTableAllocator,
       will_increment_ram_consumption_when_shared_block_is_allocated) {
  temptable::TableResourceMonitor table_resource_monitor(16 * 1024 * 1024);
  temptable::Block shared_block;
  EXPECT_TRUE(shared_block.is_empty());
  temptable::Allocator<uint8_t> allocator(&shared_block,
                                          table_resource_monitor);

  // RAM consumption is 0 at the start
  EXPECT_EQ(MemoryMonitorReadOnlyProbe::ram_consumption(), 0);

  // First allocation is fed from shared-block
  size_t shared_block_n_elements = 1024 * 1024;
  uint8_t *shared_block_ptr = allocator.allocate(shared_block_n_elements);
  EXPECT_FALSE(shared_block.is_empty());

  // RAM consumption should be greater or equal than
  // shared_block_n_elements bytes at this point
  EXPECT_GE(MemoryMonitorReadOnlyProbe::ram_consumption(),
            shared_block_n_elements);

  // Deallocate the shared-block
  allocator.deallocate(shared_block_ptr, shared_block_n_elements);

  // Physically deallocate the shared-block (allocator keeps it alive
  // intentionally)
  EXPECT_FALSE(shared_block.is_empty());
  temptable::Prefer_RAM_over_MMAP_policy::block_freed(shared_block.size(),
                                                      shared_block.type());
  shared_block.destroy();
  EXPECT_TRUE(shared_block.is_empty());
}

TEST_F(TempTableAllocator,
       will_not_decrement_ram_consumption_when_shared_block_is_deallocated) {
  temptable::TableResourceMonitor table_resource_monitor(16 * 1024 * 1024);
  temptable::Block shared_block;
  EXPECT_TRUE(shared_block.is_empty());
  temptable::Allocator<uint8_t> allocator(&shared_block,
                                          table_resource_monitor);

  // RAM consumption is 0 at the start
  EXPECT_EQ(MemoryMonitorReadOnlyProbe::ram_consumption(), 0);

  // First allocation is fed from shared-block
  size_t shared_block_n_elements = 1024 * 1024;
  uint8_t *shared_block_ptr = allocator.allocate(shared_block_n_elements);
  EXPECT_FALSE(shared_block.is_empty());

  // RAM consumption should be greater or equal than
  // shared_block_n_elements bytes at this point
  EXPECT_GE(MemoryMonitorReadOnlyProbe::ram_consumption(),
            shared_block_n_elements);

  // Deallocate the shared-block
  allocator.deallocate(shared_block_ptr, shared_block_n_elements);

  // Physically deallocate the shared-block (allocator keeps it alive
  // intentionally)
  EXPECT_FALSE(shared_block.is_empty());
  temptable::Prefer_RAM_over_MMAP_policy::block_freed(shared_block.size(),
                                                      shared_block.type());
  shared_block.destroy();
  EXPECT_TRUE(shared_block.is_empty());

  EXPECT_EQ(MemoryMonitorReadOnlyProbe::ram_consumption(), 0);
}

TEST_F(
    TempTableAllocator,
    ram_consumption_does_not_drop_to_zero_when_last_non_shared_block_is_destroyed) {
  temptable::TableResourceMonitor table_resource_monitor(16 * 1024 * 1024);
  temptable::Block shared_block;
  EXPECT_TRUE(shared_block.is_empty());
  temptable::Allocator<uint8_t> allocator(&shared_block,
                                          table_resource_monitor);

  /* Set appropriate temptable_max_mmap */
  MemoryMonitorHijackProbe::max_mmap_set(1 << 30 /* 1 GiB */);

  // RAM consumption should be greater or equal than
  // shared_block_n_elements bytes at this point
  EXPECT_EQ(MemoryMonitorReadOnlyProbe::ram_consumption(), 0);

  // Make sure we fill up the shared_block first
  // nr of elements must be >= 1MiB in size
  size_t shared_block_n_elements = 1024 * 1024 + 256;
  uint8_t *shared_block_ptr = allocator.allocate(shared_block_n_elements);
  EXPECT_FALSE(shared_block.is_empty());

  // Not even 1-byte should be able to fit anymore
  EXPECT_FALSE(shared_block.can_accommodate(1));

  // Now our next allocation should result in new block allocation ...
  size_t non_shared_block_n_elements = 2 * 1024;
  uint8_t *non_shared_block_ptr =
      allocator.allocate(non_shared_block_n_elements);

  // Make sure that pointers (Chunk's) are from different blocks
  EXPECT_NE(temptable::Block(temptable::Chunk(non_shared_block_ptr)),
            temptable::Block(temptable::Chunk(shared_block_ptr)));

  // RAM consumption should be greater or equal than
  // non_shared_block_n_elements bytes at this point
  EXPECT_GE(MemoryMonitorReadOnlyProbe::ram_consumption(),
            non_shared_block_n_elements);

  // Deallocate the non-shared block
  allocator.deallocate(non_shared_block_ptr, non_shared_block_n_elements);

  // RAM consumption should be greater or equal than
  // shared_block_n_elements bytes at this point
  EXPECT_GE(MemoryMonitorReadOnlyProbe::ram_consumption(),
            shared_block_n_elements);

  // Deallocate the shared-block
  allocator.deallocate(shared_block_ptr, shared_block_n_elements);

  // Physically deallocate the shared-block (allocator keeps it alive
  // intentionally)
  EXPECT_FALSE(shared_block.is_empty());
  temptable::Prefer_RAM_over_MMAP_policy::block_freed(shared_block.size(),
                                                      shared_block.type());
  shared_block.destroy();
  EXPECT_TRUE(shared_block.is_empty());
}

TEST_F(
    TempTableAllocator,
    shared_block_allocated_from_ram_when_ram_threshold_is_not_hit_for_given_block_size) {
  temptable::TableResourceMonitor table_resource_monitor(16 * 1024 * 1024);
  temptable::Block shared_block;
  EXPECT_TRUE(shared_block.is_empty());
  temptable::Allocator<uint8_t> allocator(&shared_block,
                                          table_resource_monitor);

  // Size of the shared_block we will request must fit (not hit the
  // threshold)
  size_t shared_block_n_elements = 1024;
  EXPECT_LE(MemoryMonitorReadOnlyProbe::ram_consumption() +
                temptable::Block::size_hint(shared_block_n_elements),
            MemoryMonitorReadOnlyProbe::ram_threshold());

  // First allocation is fed from shared-block
  uint8_t *shared_block_ptr = allocator.allocate(shared_block_n_elements);
  EXPECT_FALSE(shared_block.is_empty());

  // RAM consumption should be greater or equal than
  // shared_block_n_elements bytes at this point
  EXPECT_GE(MemoryMonitorReadOnlyProbe::ram_consumption(),
            shared_block_n_elements);

  // Deallocate the shared-block
  allocator.deallocate(shared_block_ptr, shared_block_n_elements);

  // Physically deallocate the shared-block (allocator keeps it alive
  // intentionally)
  EXPECT_FALSE(shared_block.is_empty());
  temptable::Prefer_RAM_over_MMAP_policy::block_freed(shared_block.size(),
                                                      shared_block.type());
  shared_block.destroy();
  EXPECT_TRUE(shared_block.is_empty());
}

TEST_F(
    TempTableAllocator,
    shared_block_allocated_from_mmap_when_ram_threshold_is_hit_for_given_block_size) {
  temptable::TableResourceMonitor table_resource_monitor(16 * 1024 * 1024);
  temptable::Block shared_block;
  EXPECT_TRUE(shared_block.is_empty());
  temptable::Allocator<uint8_t> allocator(&shared_block,
                                          table_resource_monitor);

  // Set some artificially low RAM threshold
  MemoryMonitorHijackProbe::max_ram_set(128);

  /* Set appropriate temptable_max_mmap */
  MemoryMonitorHijackProbe::max_mmap_set(1 << 30 /* 1 GiB */);

  // Size of the shared_block we will request must exceed the RAM threshold
  size_t shared_block_n_elements = 1024;
  EXPECT_GT(MemoryMonitorReadOnlyProbe::ram_consumption() +
                temptable::Block::size_hint(shared_block_n_elements),
            MemoryMonitorReadOnlyProbe::ram_threshold());

  // First allocation is fed from shared-block
  uint8_t *shared_block_ptr = allocator.allocate(shared_block_n_elements);
  EXPECT_FALSE(shared_block.is_empty());

  // As we have no means to track MMAP consumption yet, we will have to deduce
  // information that shared_block was allocated from MMAP by checking if
  // RAM consumption remained the same (zero)
  EXPECT_EQ(MemoryMonitorReadOnlyProbe::ram_consumption(), 0);
  // Similarly we can check that we didn't get nullptr block
  EXPECT_TRUE(shared_block_ptr != nullptr);

  // Deallocate the shared-block
  allocator.deallocate(shared_block_ptr, shared_block_n_elements);

  // Physically deallocate the shared-block (allocator keeps it alive
  // intentionally)
  EXPECT_FALSE(shared_block.is_empty());
  temptable::Prefer_RAM_over_MMAP_policy::block_freed(shared_block.size(),
                                                      shared_block.type());
  shared_block.destroy();
  EXPECT_TRUE(shared_block.is_empty());
}

TEST_F(TempTableAllocator, zero_size_allocation_returns_nullptr) {
  temptable::TableResourceMonitor table_resource_monitor(16 * 1024 * 1024);
  temptable::Allocator<uint8_t> allocator(nullptr, table_resource_monitor);
  EXPECT_EQ(nullptr, allocator.allocate(0));
}

TEST_F(TempTableAllocator, block_size_cap) {
  temptable::Block shared_block;
  EXPECT_TRUE(shared_block.is_empty());
  temptable::TableResourceMonitor table_resource_monitor(
      std::numeric_limits<size_t>::max());
  temptable::Allocator<uint8_t> allocator(&shared_block,
                                          table_resource_monitor);

  using namespace temptable;

  constexpr size_t alloc_size = 1_MiB;
  constexpr size_t n_allocate = ALLOCATOR_MAX_BLOCK_BYTES / alloc_size + 10;
  std::array<uint8_t *, n_allocate> a;

  /* Set appropriate temptable_max_mmap */
  MemoryMonitorHijackProbe::max_mmap_set(1 << 30 /* 1 GiB */);

  for (size_t i = 0; i < n_allocate; ++i) {
    a[i] = allocator.allocate(alloc_size);
  }

  EXPECT_FALSE(shared_block.is_empty());

  for (size_t i = 0; i < n_allocate; ++i) {
    allocator.deallocate(a[i], alloc_size);
  }

  // Physically deallocate the shared-block (allocator keeps it alive
  // intentionally)
  EXPECT_FALSE(shared_block.is_empty());
  temptable::Prefer_RAM_over_MMAP_policy::block_freed(shared_block.size(),
                                                      shared_block.type());
  shared_block.destroy();
  EXPECT_TRUE(shared_block.is_empty());
}

TEST_F(
    TempTableAllocator,
    table_resource_monitor_increases_then_drops_to_0_when_allocation_is_backed_by_shared_block) {
  temptable::TableResourceMonitor table_resource_monitor(16_MiB);
  temptable::Block shared_block;
  EXPECT_TRUE(shared_block.is_empty());
  temptable::Allocator<uint8_t> allocator(&shared_block,
                                          table_resource_monitor);

  // Make sure table resource monitor is set
  EXPECT_EQ(table_resource_monitor.consumption(), 0);
  EXPECT_EQ(table_resource_monitor.threshold(), 16_MiB);

  // Allocate a chunk
  auto chunk_from_shared_block = allocator.allocate(5_KiB);

  // Make sure that the chunk is fed by the shared_block
  temptable::Block block =
      temptable::Block(temptable::Chunk(chunk_from_shared_block));
  EXPECT_EQ(block, shared_block);
  EXPECT_EQ(block.size(), shared_block.size());

  // Check that the table resource monitor increased accordingly
  EXPECT_EQ(table_resource_monitor.consumption(), 5_KiB);

  // Deallocate and check that the table resource monitor decreased accordingly
  allocator.deallocate(chunk_from_shared_block, 5_KiB);
  EXPECT_EQ(table_resource_monitor.consumption(), 0_KiB);

  // Physically deallocate the shared-block (allocator keeps it alive
  // intentionally)
  EXPECT_FALSE(shared_block.is_empty());
  temptable::Prefer_RAM_over_MMAP_policy::block_freed(shared_block.size(),
                                                      shared_block.type());
  shared_block.destroy();
  EXPECT_TRUE(shared_block.is_empty());
}

TEST_F(
    TempTableAllocator,
    table_resource_monitor_increases_then_drops_to_0_when_allocation_is_not_backed_by_shared_block) {
  temptable::TableResourceMonitor table_resource_monitor(16_MiB);
  temptable::Allocator<uint8_t> allocator(nullptr, table_resource_monitor);

  // Make sure table resource monitor is set
  EXPECT_EQ(table_resource_monitor.consumption(), 0);
  EXPECT_EQ(table_resource_monitor.threshold(), 16_MiB);

  // Allocate a chunk
  auto chunk = allocator.allocate(5_KiB);

  // Check that the table resource monitor increased accordingly
  EXPECT_EQ(table_resource_monitor.consumption(), 5_KiB);

  // Deallocate and check that the table resource monitor decreased accordingly
  allocator.deallocate(chunk, 5_KiB);
  EXPECT_EQ(table_resource_monitor.consumption(), 0_KiB);
}

TEST_F(
    TempTableAllocator,
    table_resource_monitor_increases_then_drops_to_0_when_there_are_multitude_of_allocations) {
  temptable::TableResourceMonitor table_resource_monitor(16_MiB);
  temptable::Block shared_block;
  EXPECT_TRUE(shared_block.is_empty());
  temptable::Allocator<uint8_t> allocator(&shared_block,
                                          table_resource_monitor);

  // Make sure table resource monitor is set
  EXPECT_EQ(table_resource_monitor.consumption(), 0);
  EXPECT_EQ(table_resource_monitor.threshold(), 16_MiB);

  // Allocate a chunk
  auto chunk1 = allocator.allocate(5_KiB);

  // Check that the table resource monitor increased accordingly
  EXPECT_EQ(table_resource_monitor.consumption(), 5_KiB);

  // Allocate another chunk
  auto chunk2 = allocator.allocate(10_KiB);

  // Check that the table resource monitor increased accordingly
  EXPECT_EQ(table_resource_monitor.consumption(), 15_KiB);

  // Deallocate the first chunk and check that the table resource monitor
  // decreased accordingly
  allocator.deallocate(chunk1, 5_KiB);
  EXPECT_EQ(table_resource_monitor.consumption(), 10_KiB);

  // Allocate another chunk
  auto chunk3 = allocator.allocate(50_KiB);

  // Check that the table resource monitor increased accordingly
  EXPECT_EQ(table_resource_monitor.consumption(), 60_KiB);

  // Deallocate the second chunk and check that the table resource monitor
  // decreased accordingly
  allocator.deallocate(chunk2, 10_KiB);
  EXPECT_EQ(table_resource_monitor.consumption(), 50_KiB);

  // Deallocate the third chunk and check that the table resource monitor
  // decreased accordingly
  allocator.deallocate(chunk3, 50_KiB);
  EXPECT_EQ(table_resource_monitor.consumption(), 0_KiB);

  // Physically deallocate the shared-block (allocator keeps it alive
  // intentionally)
  EXPECT_FALSE(shared_block.is_empty());
  temptable::Prefer_RAM_over_MMAP_policy::block_freed(shared_block.size(),
                                                      shared_block.type());
  shared_block.destroy();
  EXPECT_TRUE(shared_block.is_empty());
}

TEST_F(
    TempTableAllocator,
    table_resource_monitor_limit_is_respected_and_record_file_full_is_thrown) {
  temptable::TableResourceMonitor table_resource_monitor(2_MiB);
  temptable::Block shared_block;
  EXPECT_TRUE(shared_block.is_empty());
  temptable::Allocator<uint8_t> allocator(&shared_block,
                                          table_resource_monitor);

  // Make sure table resource monitor is set
  EXPECT_EQ(table_resource_monitor.consumption(), 0);
  EXPECT_EQ(table_resource_monitor.threshold(), 2_MiB);

  // Allocate a chunk
  auto chunk1 = allocator.allocate(792_KiB);

  // Check that the table resource monitor increased accordingly
  EXPECT_EQ(table_resource_monitor.consumption(), 792_KiB);

  // Allocate another chunk
  auto chunk2 = allocator.allocate(512_KiB);

  // Check that the table resource monitor increased accordingly
  EXPECT_EQ(table_resource_monitor.consumption(), 792_KiB + 512_KiB);

  try {
    // Allocate another chunk
    auto chunk3 = allocator.allocate(1024_KiB);
    (void)chunk3;
  } catch (std::exception &) {
    EXPECT_TRUE(false);
  } catch (temptable::Result r) {
    EXPECT_EQ(r, temptable::Result::RECORD_FILE_FULL);
  }

  // Deallocate the second chunk and check that the table resource monitor
  // decreased accordingly
  allocator.deallocate(chunk2, 512_KiB);
  EXPECT_EQ(table_resource_monitor.consumption(), 792_KiB);

  // Deallocate the third chunk and check that the table resource monitor
  // decreased accordingly
  allocator.deallocate(chunk1, 792_KiB);
  EXPECT_EQ(table_resource_monitor.consumption(), 0_KiB);

  // Physically deallocate the shared-block (allocator keeps it alive
  // intentionally)
  EXPECT_FALSE(shared_block.is_empty());
  temptable::Prefer_RAM_over_MMAP_policy::block_freed(shared_block.size(),
                                                      shared_block.type());
  shared_block.destroy();
  EXPECT_TRUE(shared_block.is_empty());
}

TEST_F(TempTableAllocator,
       shared_block_utilization_shall_not_impact_the_block_size_growth_policy) {
  temptable::TableResourceMonitor table_resource_monitor(16 * 1024 * 1024);
  temptable::Block shared_block;
  temptable::Allocator<uint8_t> a1(&shared_block, table_resource_monitor);
  temptable::Allocator<uint8_t> a2(&shared_block, table_resource_monitor);

  /* Set appropriate temptable_max_mmap */
  MemoryMonitorHijackProbe::max_mmap_set(1 << 30 /* 1 GiB */);

  auto r11 = a1.allocate(512_KiB);
  temptable::Block b11 = temptable::Block(temptable::Chunk(r11));
  EXPECT_EQ(b11, shared_block);
  EXPECT_EQ(b11.size(), shared_block.size());
  EXPECT_EQ(b11.size(), 1_MiB);
  // ^^
  // 1. Allocator detects that shared_block is empty
  // 2. It uses the block-size growth policy to compute the block-size
  // 3. It allocates the block of 1MiB of size. Our shared_block is now 1MiB of
  // size big.
  // 4. Returns a pointer from shared_block.

  auto r12 = a1.allocate(256_KiB);
  temptable::Block b12 = temptable::Block(temptable::Chunk(r12));
  EXPECT_EQ(b12, shared_block);
  EXPECT_EQ(b12.size(), shared_block.size());
  // ^^
  // 1. Allocator detects that shared_block is not empty
  // 2. It detects that shared_block has enough space left (1MiB - 512KiB =
  // 512KiB) to accomodate the 256KiB request.
  // 3. Returns a pointer from shared_block.

  auto r13 = a1.allocate(512_KiB);
  temptable::Block b13 = temptable::Block(temptable::Chunk(r13));
  EXPECT_NE(b13, shared_block);
  EXPECT_NE(b13, b12);
  EXPECT_EQ(b13.size(), 1_MiB);
  // ^^
  // 1. Allocator detects that shared_block is not empty
  // 2. It detects that shared_block does not have enough space left (1MiB -
  // 512KiB - 256KiB = 256KiB) to accomodate the 512KiB request.
  // 3. It uses the block-size growth policy to compute the block-size.
  // 4. It allocates the block of 2MiB of size.
  // 5. Returns a pointer from new block.

  auto r21 = a2.allocate(512_KiB);
  temptable::Block b21 = temptable::Block(temptable::Chunk(r21));
  EXPECT_NE(b21, shared_block);
  EXPECT_EQ(b21.size(), 1_MiB);
  // ^^^^
  // 1. Allocator detects that shared_block is not empty.
  // 2. It detects that shared_block does not have enough space left (1MiB -
  // 512KiB - 256KiB = 256KiB) to accomodate the 512KiB request.
  // 3. It uses the block-size growth policy to compute the block-size.
  // 4. It allocates the block of 1MiB of size.
  // 5. Returns a pointer from new block.

  auto r14 = a1.allocate(128_KiB);
  temptable::Block b14 = temptable::Block(temptable::Chunk(r14));
  EXPECT_EQ(b14, shared_block);
  EXPECT_EQ(b14.size(), shared_block.size());
  // ^^
  // 1. Allocator detects that shared_block is not empty
  // 2. It detects that shared_block has enough space left (1MiB - 512KiB -
  // 256KiB = 256KiB) to accomodate the 128KiB request.
  // 3. Returns a pointer from shared_block.

  auto r15 = a1.allocate(1_MiB - 512_KiB);
  temptable::Block b15 = temptable::Block(temptable::Chunk(r15));
  EXPECT_NE(b15, shared_block);
  EXPECT_EQ(b15.size(), 2_MiB);
  // ^^
  // 1. Allocator detects that shared_block is not empty.
  // 2. It detects that shared_block does not have enough space left (1MiB -
  // 512KiB - 256KiB - 128KiB = 128KiB) to accomodate the 1.5MiB request.
  // 3. It also checks if there is enough space left in secondly instantiated
  // 1MiB block (see (B)) to accomodate the 1.5MiB. It does not.
  // 4. It allocates the block of 2MiB of size.
  // 3. Returns a pointer from new block.

  auto r22 = a2.allocate(1_MiB);
  temptable::Block b22 = temptable::Block(temptable::Chunk(r22));
  EXPECT_NE(b22, shared_block);
  EXPECT_EQ(b22.size(), 2_MiB);
  // 1. Allocator detects that shared_block is not empty.
  // 2. It detects that shared_block does not have enough space left (1MiB -
  // 512KiB - 256KiB - 128KiB = 128KiB) to accomodate the 1MiB request.
  // 3. It uses the block-size growth policy to compute the block-size.
  // 4. It allocates the block of 2MiB of size.
  // 5. Returns a pointer from new block.

  a1.deallocate(r11, 512_KiB);
  a1.deallocate(r12, 256_KiB);
  a1.deallocate(r13, 512_KiB);
  a1.deallocate(r14, 128_KiB);
  a1.deallocate(r15, 1_MiB - 512_KiB);
  a2.deallocate(r21, 512_KiB);
  a2.deallocate(r22, 1_MiB);

  // Physically deallocate the shared-block (allocator keeps it alive
  // intentionally)
  EXPECT_FALSE(shared_block.is_empty());
  temptable::Prefer_RAM_over_MMAP_policy::block_freed(shared_block.size(),
                                                      shared_block.type());
  shared_block.destroy();
  EXPECT_TRUE(shared_block.is_empty());
}

TEST_F(
    TempTableAllocator,
    repeated_allocation_followed_by_deallocation_does_not_create_new_blocks) {
  temptable::TableResourceMonitor table_resource_monitor(16 * 1024 * 1024);
  {
    temptable::Block shared_block;
    ;
    temptable::Allocator<uint8_t> allocator(&shared_block,
                                            table_resource_monitor);
    // RAM consumption is 0 at the start
    EXPECT_EQ(MemoryMonitorReadOnlyProbe::ram_consumption(), 0);

    auto r1 = allocator.allocate(800_KiB);
    temptable::Block b1 = temptable::Block(temptable::Chunk(r1));
    EXPECT_EQ(b1, shared_block);
    EXPECT_EQ(b1.size(), shared_block.size());
    EXPECT_EQ(b1.size(), 1_MiB);
    // ^^
    // 1. Allocator detects that shared_block is empty
    // 2. It uses the block-size growth policy to compute the block-size
    // 3. It allocates the block of 1MiB of size. Our shared_block is now 1MiB
    // of size big.
    // 4. Returns a pointer from shared_block.

    auto r2 = allocator.allocate(800_KiB);
    temptable::Block b2 = temptable::Block(temptable::Chunk(r2));
    EXPECT_NE(b2, shared_block);
    EXPECT_EQ(b2.size(), 1_MiB);
    EXPECT_EQ(MemoryMonitorReadOnlyProbe::ram_consumption(), 2_MiB);
    // ^^
    // 1. Allocator detects that shared_block is not empty, but it can't use it
    // to allocate new chunk.
    // 2. It allocates a new block of 1MiB of size.
    // 3. Returns a pointer from a new block.

    {
      auto r3 = allocator.allocate(800_KiB);
      temptable::Block b3 = temptable::Block(temptable::Chunk(r3));
      EXPECT_NE(b3, shared_block);
      EXPECT_EQ(b3.size(), 2_MiB);
      EXPECT_EQ(MemoryMonitorReadOnlyProbe::ram_consumption(), 4_MiB);
      // ^^
      // 1. Allocator detects that shared_block is not empty, but it can't use
      // it to allocate new chunk.
      // 2. Neither the current block can be used.
      // 3. It allocates a new block of 1MiB of size.
      // 4. Returns a pointer from a new block.

      allocator.deallocate(r3, 800_KiB);
      EXPECT_EQ(MemoryMonitorReadOnlyProbe::ram_consumption(), 4_MiB);
      // ^^
      // 1. Allocator removes the chunk from the current block.
      // 2. It sees it is now empty, but caches it and does not deallocate it.
      // 3. The consumption stays at 4MiB.
    }

    {
      auto r3 = allocator.allocate(800_KiB);
      temptable::Block b3 = temptable::Block(temptable::Chunk(r3));
      EXPECT_NE(b3, shared_block);
      EXPECT_EQ(b3.size(), 2_MiB);
      EXPECT_EQ(MemoryMonitorReadOnlyProbe::ram_consumption(), 4_MiB);
      // ^^
      // 1. Allocator detects that shared_block is not empty, but it can't use
      // it to allocate new chunk.
      // 2. The current block can be used as it empty now.
      // 3. It allocates a new block of 1MiB of size.
      // 4. Returns a pointer from a new block.

      allocator.deallocate(r3, 800_KiB);
      EXPECT_EQ(MemoryMonitorReadOnlyProbe::ram_consumption(), 4_MiB);
      // ^^
      // 1. Allocator removes the chunk from the current block.
      // 2. It sees it is now empty, but caches it and does not deallocate it.
      // 3. The consumption stays at 4MiB.
    }
    allocator.deallocate(r2, 800_KiB);
    EXPECT_EQ(MemoryMonitorReadOnlyProbe::ram_consumption(), 3_MiB);
    // ^^
    // 1. Allocator removes the chunk from old block.
    // 2. It sees it is now empty, and is not the current one and deallocates
    // it.
    // 3. The consumption drops to 2MiB.
    allocator.deallocate(r1, 800_KiB);

    // Physically deallocate the shared-block (allocator keeps it alive
    // intentionally)
    EXPECT_FALSE(shared_block.is_empty());
    temptable::Prefer_RAM_over_MMAP_policy::block_freed(shared_block.size(),
                                                        shared_block.type());
    shared_block.destroy();
    EXPECT_TRUE(shared_block.is_empty());

    EXPECT_EQ(MemoryMonitorReadOnlyProbe::ram_consumption(), 2_MiB);
    // ^^
    // 1. Shared block is deallocated.
    // 2. Allocator still holds the current block alive.
    // 3. The consumption should drop by the 1MiB used by the shared block.
  }
  EXPECT_EQ(MemoryMonitorReadOnlyProbe::ram_consumption(), 0_MiB);
  // ^^
  // 1. Allocator is destroyed.
  // 2. It sees it has an empty current block and deallocates it.
  // 3. The consumption should drop by the 2MiB used by the current block.
}

// Create some aliases to make our life easier when generating the test-cases
// down below.
using max_ram = decltype(temptable_max_ram);
using max_mmap = decltype(temptable_max_mmap);
using use_mmap = decltype(temptable_use_mmap);
using n_elements = uint32_t;
using is_ram_expected_to_be_increased = bool;
using is_mmap_expected_to_be_increased = bool;

// Parametrized test for testing successful allocation patterns
class AllocatesSuccessfully
    : public TempTableAllocator,
      public ::testing::WithParamInterface<std::tuple<
          max_ram, max_mmap, use_mmap, n_elements,
          is_ram_expected_to_be_increased, is_mmap_expected_to_be_increased>> {
};

// Parametrized test for testing allocation patterns which should yield
// RecordFileFull exception
class ThrowsRecordFileFull
    : public TempTableAllocator,
      public ::testing::WithParamInterface<
          std::tuple<max_ram, max_mmap, use_mmap, n_elements>> {};

// Implementation of parametrized test-cases which tests successful allocation
// patterns
TEST_P(AllocatesSuccessfully,
       for_various_allocation_patterns_and_configurations) {
  auto max_ram = std::get<0>(GetParam());
  auto max_mmap = std::get<1>(GetParam());
  auto mmap_enabled = std::get<2>(GetParam());
  auto n_elements = std::get<3>(GetParam());
  auto ram_expected_to_be_increased = std::get<4>(GetParam());
  auto mmap_expected_to_be_increased = std::get<5>(GetParam());

  MemoryMonitorHijackProbe::max_ram_set(max_ram);
  MemoryMonitorHijackProbe::max_mmap_set(max_mmap);
  mmap_enabled ? MemoryMonitorHijackProbe::mmap_enable()
               : MemoryMonitorHijackProbe::mmap_disable();

  // Trigger the allocation
  temptable::TableResourceMonitor table_resource_monitor(16 * 1024 * 1024);
  temptable::Allocator<uint8_t> allocator(nullptr, table_resource_monitor);
  uint8_t *chunk = nullptr;
  EXPECT_NO_THROW(chunk = allocator.allocate(n_elements));
  EXPECT_NE(chunk, nullptr);

  // After successful allocation, and depending on the use-case, RAM and MMAP
  // consumption should increase or stay at the same level accordingly
  if (ram_expected_to_be_increased) {
    EXPECT_GE(MemoryMonitorReadOnlyProbe::ram_consumption(),
              n_elements * sizeof(uint8_t));
  } else {
    EXPECT_EQ(MemoryMonitorReadOnlyProbe::ram_consumption(), 0);
  }
  if (mmap_expected_to_be_increased) {
    EXPECT_GE(MemoryMonitorReadOnlyProbe::mmap_consumption(),
              n_elements * sizeof(uint8_t));
  } else {
    EXPECT_EQ(MemoryMonitorReadOnlyProbe::mmap_consumption(), 0);
  }

  // Clean-up
  allocator.deallocate(chunk, n_elements);
}

// Implementation of parametrized test-cases which tests allocation patterns
// yielding RecordFileFull exception
TEST_P(ThrowsRecordFileFull,
       for_various_allocation_patterns_and_configurations) {
  auto max_ram = std::get<0>(GetParam());
  auto max_mmap = std::get<1>(GetParam());
  auto mmap_enabled = std::get<2>(GetParam());
  auto n_elements = std::get<3>(GetParam());

  MemoryMonitorHijackProbe::max_ram_set(max_ram);
  MemoryMonitorHijackProbe::max_mmap_set(max_mmap);
  mmap_enabled ? MemoryMonitorHijackProbe::mmap_enable()
               : MemoryMonitorHijackProbe::mmap_disable();

  // Trigger the allocation
  temptable::TableResourceMonitor table_resource_monitor(16 * 1024 * 1024);
  temptable::Allocator<uint8_t> allocator(nullptr, table_resource_monitor);
  uint8_t *chunk = nullptr;
  EXPECT_THROW_WITH_VALUE(chunk = allocator.allocate(n_elements),
                          temptable::Result,
                          temptable::Result::RECORD_FILE_FULL);
  EXPECT_EQ(chunk, nullptr);
  // After allocation failure, RAM consumption must remain intact (zero)
  EXPECT_EQ(MemoryMonitorReadOnlyProbe::ram_consumption(), 0);
  // Ditto for MMAP
  EXPECT_EQ(MemoryMonitorReadOnlyProbe::mmap_consumption(), 0);
}

// Generate tests for all of the test-case scenarios which should yield
// RecordFileFull exception
INSTANTIATE_TEST_SUITE_P(
    TempTableAllocator, ThrowsRecordFileFull,
    ::testing::Values(
        // ram threshold reached, mmap threshold not reached, mmap disabled
        std::make_tuple(1_MiB, 2_MiB, false, 1_MiB + 1),
        // ram threshold reached, mmap threshold reached, mmap disabled
        std::make_tuple(1_MiB, 1_MiB, false, 2_MiB),
        // ram threshold reached, mmap threshold reached, mmap enabled
        std::make_tuple(1_MiB, 1_MiB, true, 2_MiB),
        // ram threshold reached, mmap threshold reached (but set to 0), mmap
        // disabled
        std::make_tuple(1_MiB, 0_MiB, false, 2_MiB),
        // ram threshold reached, mmap threshold reached (but set to 0), mmap
        // enabled
        std::make_tuple(1_MiB, 0_MiB, true, 2_MiB)));

// Generate tests for all of the test-case scenarios which should result with a
// successful allocation
INSTANTIATE_TEST_SUITE_P(
    TempTableAllocator, AllocatesSuccessfully,
    ::testing::Values(
        // ram threshold not reached, mmap threshold not reached (but set to 0),
        // mmap disabled
        std::make_tuple(1_MiB, 0_MiB, false, 2_KiB, true, false),
        // ram threshold not reached, mmap threshold not reached (but set to 0),
        // mmap enabled
        std::make_tuple(1_MiB, 0_MiB, true, 2_KiB, true, false),
        // ram threshold not reached, mmap threshold not reached, mmap disabled
        std::make_tuple(1_MiB, 1_MiB, true, 2_KiB, true, false),
        // ram threshold not reached, mmap threshold not reached, mmap enabled
        std::make_tuple(1_MiB, 1_MiB, true, 2_KiB, true, false),
        // ram threshold reached, mmap threshold not reached, mmap enabled
        std::make_tuple(1_MiB, 4_MiB, true, 2_MiB, false, true)));

// Create some aliases to make our life easier when generating the test-cases
// down below.
using block_size_expected = size_t;
using block_size = uint32_t;
using number_of_blocks = size_t;
using n_bytes_requested = size_t;
using ram_consumption = uint32_t;
using ram_threshold = uint32_t;
using mmap_threshold = uint32_t;
using exception_will_be_thrown = bool;
using expected_source = temptable::Source;

// Parameterized test for testing Exponential_policy behavior in cases when
// requested block size is smaller than the power to the two number which
// will be internally picked up by the policy.
class ExponentialPolicyReturnsPowerToTheTwoBlockSize
    : public TempTableAllocator,
      public ::testing::WithParamInterface<std::tuple<
          number_of_blocks, n_bytes_requested, block_size_expected>> {};

// Implementation of parameterized test-cases which test for correct block sizes
// returned by policy.
TEST_P(ExponentialPolicyReturnsPowerToTheTwoBlockSize,
       when_actual_block_size_is_smaller_than_that_power_to_the_two_number) {
  auto number_of_blocks = std::get<0>(GetParam());
  auto n_bytes_requested = std::get<1>(GetParam());
  auto block_size_expected = std::get<2>(GetParam());

  EXPECT_EQ(block_size_expected, temptable::Exponential_policy::block_size(
                                     number_of_blocks, n_bytes_requested));
}

// Generate the test-case scenarios.
INSTANTIATE_TEST_SUITE_P(
    TempTableBlockSizePolicy1, ExponentialPolicyReturnsPowerToTheTwoBlockSize,
    ::testing::Values(
        // First and smallest block size returned is always 1 MiB
        // (unless requested size is larger than 1 MiB)
        std::make_tuple(0, 1_KiB, 1_MiB),
        // ...
        std::make_tuple(0, 5_KiB, 1_MiB),
        // ...
        std::make_tuple(0, 128_KiB, 1_MiB),
        // ...
        std::make_tuple(0, 512_KiB, 1_MiB),
        // ...
        std::make_tuple(0, 786_KiB, 1_MiB),
        // Block size returned will grow exponentially if we continue
        // increasing number of blocks (first) parameter
        std::make_tuple(1, 1_KiB, 2_MiB),
        // ...
        std::make_tuple(2, 1_KiB, 4_MiB),
        // ...
        std::make_tuple(3, 1_KiB, 8_MiB),
        // ...
        std::make_tuple(4, 1_KiB, 16_MiB),
        // ...
        std::make_tuple(5, 1_KiB, 32_MiB),
        // ...
        std::make_tuple(6, 1_KiB, 64_MiB),
        // ...
        std::make_tuple(7, 1_KiB, 128_MiB),
        // ...
        std::make_tuple(8, 1_KiB, 256_MiB),
        // Once number of blocks hits the
        // temptable::ALLOCATOR_MAX_BLOCK_MB_EXP threshold, block size of
        // temptable::ALLOCATOR_MAX_BLOCK_BYTES will be returned if
        // requested size is not bigger than that
        std::make_tuple(temptable::ALLOCATOR_MAX_BLOCK_MB_EXP, 1_MiB,
                        temptable::ALLOCATOR_MAX_BLOCK_BYTES)));

// Parameterized test for testing Exponential_policy behavior in cases when
// requested block size is larger than the power to the two number which
// would be otherwise internally picked up by the policy.
class ExponentialPolicyReturnsExactBlockSize
    : public TempTableAllocator,
      public ::testing::WithParamInterface<std::tuple<
          number_of_blocks, n_bytes_requested, block_size_expected>> {};

// Implementation of parameterized test-cases which test for correct block sizes
// returned by policy.
TEST_P(
    ExponentialPolicyReturnsExactBlockSize,
    when_actual_block_size_is_larger_than_the_power_to_the_two_number_which_would_be_otherwise_used) {
  auto number_of_blocks = std::get<0>(GetParam());
  auto n_bytes_requested = std::get<1>(GetParam());
  auto block_size_expected = std::get<2>(GetParam());

  EXPECT_EQ(block_size_expected, temptable::Exponential_policy::block_size(
                                     number_of_blocks, n_bytes_requested));
}

// Generate the test-case scenarios.
INSTANTIATE_TEST_SUITE_P(
    TempTableBlockSizePolicy2, ExponentialPolicyReturnsExactBlockSize,
    ::testing::Values(
        // If requested size is larger than 1 MiB, then returned size must match
        // the expected size and not the number which is power to the 2
        std::make_tuple(0, 1_MiB, temptable::Block::size_hint(1_MiB)),
        // Same for any other combination of number of blocks input
        std::make_tuple(4, 32_MiB, temptable::Block::size_hint(32_MiB)),
        // ...
        std::make_tuple(6, 256_MiB, temptable::Block::size_hint(256_MiB)),
        // Once number of blocks hits the
        // temptable::ALLOCATOR_MAX_BLOCK_MB_EXP threshold, and requested
        // block size is larger than temptable::ALLOCATOR_MAX_BLOCK_BYTES,
        // big enough block size shall be returned.
        std::make_tuple(temptable::ALLOCATOR_MAX_BLOCK_MB_EXP,
                        temptable::ALLOCATOR_MAX_BLOCK_BYTES,
                        temptable::Block::size_hint(
                            temptable::ALLOCATOR_MAX_BLOCK_BYTES))));

// Parameterized test for testing Prefer_RAM_over_MMAP_policy behavior.
class PreferRamOverMmapPolicy
    : public TempTableAllocator,
      public ::testing::WithParamInterface<
          std::tuple<block_size, ram_consumption, ram_threshold, mmap_threshold,
                     exception_will_be_thrown, expected_source>> {};

// Implementation of parameterized test-cases which test for correct block
// source returned by policy.
TEST_P(
    PreferRamOverMmapPolicy,
    selects_ram_or_mmap_when_requested_block_size_fits_otherwise_throws_an_exception) {
  auto block_size = std::get<0>(GetParam());
  auto ram_consumption = std::get<1>(GetParam());
  auto ram_threshold = std::get<2>(GetParam());
  auto mmap_threshold = std::get<3>(GetParam());
  auto exception_will_be_thrown = std::get<4>(GetParam());
  auto source_expected = std::get<5>(GetParam());

  MemoryMonitorHijackProbe::max_ram_set(ram_threshold);
  MemoryMonitorHijackProbe::max_mmap_set(mmap_threshold);
  MemoryMonitorHijackProbe::ram_consumption_set(ram_consumption);

  if (exception_will_be_thrown) {
    EXPECT_THROW_WITH_VALUE(
        temptable::Prefer_RAM_over_MMAP_policy::block_source(block_size),
        temptable::Result, temptable::Result::RECORD_FILE_FULL);
  } else {
    EXPECT_EQ(source_expected,
              temptable::Prefer_RAM_over_MMAP_policy::block_source(block_size));
    // A block source was successfully provisioned, and the usage was recorded.
    // Test the accounting when the block is freed.
    temptable::Prefer_RAM_over_MMAP_policy::block_freed(block_size,
                                                        source_expected);
  }

  // Reset the usage and check it is back to where we started.
  EXPECT_EQ(temptable::MemoryMonitor::RAM::consumption(), ram_consumption);
  MemoryMonitorHijackProbe::ram_consumption_reset();
}

// Generate the test-case scenarios.
INSTANTIATE_TEST_SUITE_P(
    TempTableBlockSourcePolicy, PreferRamOverMmapPolicy,
    ::testing::Values(
        // RAM threshold not reached, block size will fit, source is RAM
        std::make_tuple(1_MiB, 1_MiB, 2_MiB, 2_MiB, false,
                        temptable::Source::RAM),
        // RAM threshold not reached, block size will hit the threshold (by only
        // 1 byte), source is MMAP
        std::make_tuple(1_MiB + 1, 1_MiB, 2_MiB, 2_MiB, false,
                        temptable::Source::MMAP_FILE),
        // RAM threshold not reached, block size will hit the threshold (by
        // 1 MiB), source is MMAP
        std::make_tuple(2_MiB, 1_MiB, 2_MiB, 2_MiB, false,
                        temptable::Source::MMAP_FILE),
        // block does not fit nor in RAM or MMAP, exception will be thrown
        std::make_tuple(3_MiB, 1_MiB, 2_MiB, 2_MiB, true,
                        temptable::Source::MMAP_FILE)));

} /* namespace temptable_allocator_unittest */
