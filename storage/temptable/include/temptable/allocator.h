/* Copyright (c) 2016, 2022, Oracle and/or its affiliates.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2.0, as published by the
Free Software Foundation.

This program is also distributed with certain software (including but not
limited to OpenSSL) that is licensed under separate terms, as designated in a
particular file or component or in included license documentation. The authors
of MySQL hereby grant you an additional permission to link the program and
your derivative works with the separately licensed software that they have
included with MySQL.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License, version 2.0,
for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/** @file storage/temptable/include/temptable/allocator.h
TempTable custom allocator. */

#ifndef TEMPTABLE_ALLOCATOR_H
#define TEMPTABLE_ALLOCATOR_H

#include <algorithm>  // std::max
#include <cstddef>    // size_t
#include <limits>     // std::numeric_limits
#include <memory>     // std::shared_ptr
#include <new>        // new
#include <utility>    // std::forward

#include "my_dbug.h"
#include "my_sys.h"
#include "sql/mysqld.h"  // temptable_max_ram, temptable_max_mmap
#include "storage/temptable/include/temptable/block.h"
#include "storage/temptable/include/temptable/chunk.h"
#include "storage/temptable/include/temptable/constants.h"
#include "storage/temptable/include/temptable/memutils.h"

namespace temptable {

/* Thin abstraction which enables logging of memory operations.
 *
 * Used by the Allocator to implement switching from RAM to MMAP-backed
 * allocations and vice-versa. E.g. Allocator will switch to MMAP-backed
 * allocation strategy once temptable RAM-consumption threshold, which is
 * defined by temptable_max_ram user-modifiable variable, is reached.
 **/
struct MemoryMonitor {
  struct RAM {
    /** Log increments of heap-memory consumption.
     *
     * [in] Number of bytes.
     * @return Heap-memory consumption after increase. */
    static size_t increase(size_t bytes) {
      assert(ram <= std::numeric_limits<decltype(bytes)>::max() - bytes);
      return ram.fetch_add(bytes) + bytes;
    }
    /** Log decrements of heap-memory consumption.
     *
     * [in] Number of bytes.
     * @return Heap-memory consumption after decrease. */
    static size_t decrease(size_t bytes) {
      assert(ram >= bytes);
      return ram.fetch_sub(bytes) - bytes;
    }
    /** Get heap-memory threshold level. Level is defined by this Allocator.
     *
     * @return Heap-memory threshold. */
    static size_t threshold() { return temptable_max_ram; }
    /** Get current level of heap-memory consumption.
     *
     * @return Current level of heap-memory consumption (in bytes). */
    static size_t consumption() { return ram; }
  };

  struct MMAP {
    /** Log increments of MMAP-backed memory consumption.
     *
     * [in] Number of bytes.
     * @return MMAP-memory consumption after increase. */
    static size_t increase(size_t bytes) {
      assert(mmap <= std::numeric_limits<decltype(bytes)>::max() - bytes);
      return mmap.fetch_add(bytes) + bytes;
    }
    /** Log decrements of MMAP-backed memory consumption.
     *
     * [in] Number of bytes.
     * @return MMAP-memory consumption after decrease. */
    static size_t decrease(size_t bytes) {
      assert(mmap >= bytes);
      return mmap.fetch_sub(bytes) - bytes;
    }
    /** Get MMAP-backed memory threshold level. Level is defined by this
     * Allocator.
     *
     * @return MMAP-memory threshold. */
    static size_t threshold() {
      if (temptable_use_mmap) {
        return temptable_max_mmap;
      } else {
        return 0;
      }
    }
    /** Get current level of MMAP-backed memory consumption.
     *
     * @return Current level of MMAP-backed memory consumption (in bytes). */
    static size_t consumption() { return mmap; }
  };

 private:
  /** Total bytes allocated so far by all threads in RAM/MMAP. */
  static std::atomic<size_t> ram;
  static std::atomic<size_t> mmap;
};

/* Thin abstraction which enables logging of how much resources have been
 * consumed at the per-table level. Each temptable::Table will be composed
 * of this type so that the temptable::Allocator through its policies can
 * monitor its memory consumption and act appropriately when threshold
 * is reached.
 **/
class TableResourceMonitor {
 public:
  TableResourceMonitor(size_t threshold)
      : m_threshold(threshold), m_total_bytes(0) {}

  size_t increase(size_t bytes) {
    assert(m_total_bytes <=
           std::numeric_limits<decltype(bytes)>::max() - bytes);
    m_total_bytes += bytes;
    return m_total_bytes;
  }
  size_t decrease(size_t bytes) {
    assert(m_total_bytes >= bytes);
    m_total_bytes -= bytes;
    return m_total_bytes;
  }
  size_t threshold() { return m_threshold; }
  size_t consumption() { return m_total_bytes; }

 private:
  size_t m_threshold;
  size_t m_total_bytes;
};

/* Allocation scheme, a type which controls allocation patterns in TempTable
 * allocator.
 *
 * In particular, allocation scheme can define the behavior of TempTable
 * allocator allocations with respect to the following:
 *  1. Where each consecutive Block of memory is going to be allocated from
 *     (e.g. RAM vs MMAP vs etc.)
 *  2. How big each consecutive Block of memory is going to be
 *     (e.g. monotonic growth, exponential growth, no growth, etc.)
 *
 * Concrete implementations of previous points must be provided through
 * customization points, namely Block_size_policy and Block_source_policy,
 * template type parameters. Whatever these types are, they must provide
 * conforming interface implementations.
 *
 * Block_size_policy customization point must provide concrete implementation
 * with the following signature:
 *      static size_t block_size(size_t, size_t);
 * Similarly, concrete implementations of Block_source_policy must provide:
 *      static Source block_source(size_t);
 *
 * That allows us to build different concrete allocation schemes by simply
 * composing different customization points. For example:
 *
 *  using Monotonic_growth_RAM_only =
 *      Allocation_scheme<Monotonic_policy, RAM_only_policy>;
 *
 *  using Exponential_growth_RAM_only =
 *      Allocation_scheme<Exponential_policy, RAM_only_policy>;
 *
 *  using Exponential_growth_preferring_RAM_over_MMAP =
 *      Allocation_scheme<Exponential_policy, Prefer_RAM_over_MMAP_policy>;
 *
 *  using No_growth_RAM_only =
 *      Allocation_scheme<No_growth_policy, RAM_only_policy>;
 *
 *  etc. etc.
 *
 */
template <typename Block_size_policy, typename Block_source_policy>
struct Allocation_scheme {
  static Source block_source(size_t block_size,
                             TableResourceMonitor *table_resource_monitor) {
    return Block_source_policy::block_source(block_size,
                                             table_resource_monitor);
  }
  static size_t block_size(size_t number_of_blocks, size_t n_bytes_requested) {
    return Block_size_policy::block_size(number_of_blocks, n_bytes_requested);
  }
};

/* Concrete implementation of Block_source_policy, a type which controls where
 * TempTable allocator is going to be allocating next Block of memory from.
 *
 * In particular, this policy will make TempTable allocator:
 *  1. Use RAM as long as temptable_max_ram threshold is not reached.
 *  2. Start using MMAP when temptable_max_ram threshold is reached.
 *  3. Go back using RAM as soon as RAM consumption drops below the
 *     temptable_max_ram threshold and there is enough space to accommodate the
 *     new block given the size.
 *  4. Not take into account per-table memory limits defined through
 *     tmp_table_size SYSVAR.
 * */
struct Prefer_RAM_over_MMAP_policy {
  static Source block_source(uint32_t block_size,
                             TableResourceMonitor * = nullptr) {
    if (MemoryMonitor::RAM::consumption() < MemoryMonitor::RAM::threshold()) {
      if (MemoryMonitor::RAM::increase(block_size) <=
          MemoryMonitor::RAM::threshold()) {
        return Source::RAM;
      } else {
        MemoryMonitor::RAM::decrease(block_size);
      }
    }
    if (MemoryMonitor::MMAP::consumption() < MemoryMonitor::MMAP::threshold()) {
      if (MemoryMonitor::MMAP::increase(block_size) <=
          MemoryMonitor::MMAP::threshold()) {
        return Source::MMAP_FILE;
      } else {
        MemoryMonitor::MMAP::decrease(block_size);
      }
    }
    throw Result::RECORD_FILE_FULL;
  }
};

/* Another concrete implementation of Block_source_policy, a type which controls
 * where TempTable allocator is going to be allocating next Block of memory
 * from. It acts the same as Prefer_RAM_over_MMAP_policy with the main
 * difference being that this policy obeys the per-table limit.
 *
 * What this means is that each temptable::Table is allowed to fit no more data
 * than the given threshold controlled through TableResourceMonitor abstraction.
 * TableResourceMonitor is a simple abstraction which is in its part an alias
 * for tmp_table_size, a system variable that end MySQL users will be using to
 * control this threshold.
 *
 * Updating the tmp_table_size threshold can only be done through the separate
 * SET statement which implies that the tmp_table_size threshold cannot be
 * updated during the duration of some query which is running within the same
 * session. Separate sessions can still of course change this value to their
 * liking.
 * */
struct Prefer_RAM_over_MMAP_policy_obeying_per_table_limit {
  static Source block_source(uint32_t block_size,
                             TableResourceMonitor *table_resource_monitor) {
    assert(table_resource_monitor);
    assert(table_resource_monitor->consumption() <=
           table_resource_monitor->threshold());

    if (table_resource_monitor->consumption() + block_size >
        table_resource_monitor->threshold())
      throw Result::RECORD_FILE_FULL;

    return Prefer_RAM_over_MMAP_policy::block_source(block_size);
  }
};

/* Concrete implementation of Block_size_policy, a type which controls how big
 * next Block of memory is going to be allocated by TempTable allocator.
 *
 * In particular, this policy will make TempTable allocator to grow the
 * block-size at exponential rate with upper limit of ALLOCATOR_MAX_BLOCK_BYTES,
 * which is 2 ^ ALLOCATOR_MAX_BLOCK_MB_EXP.
 *
 * E.g. allocation pattern may look like the following:
 *  1 MiB,
 *  2 MiB,
 *  4 MiB,
 *  8 MiB,
 *  16 MiB,
 *  32 MiB,
 *  ...,
 *  ALLOCATOR_MAX_BLOCK_BYTES,
 *  ALLOCATOR_MAX_BLOCK_BYTES
 *
 * In cases when block size that is being requested is bigger than the one which
 * is calculated by this policy, requested block size will be returned (even if
 * it grows beyond ALLOCATOR_MAX_BLOCK_BYTES).
 * */
struct Exponential_policy {
  /** Given the current number of allocated blocks by the allocator, and number
   * of bytes actually requested by the client code, calculate the new block
   * size.
   *
   * [in] Current number of allocated blocks.
   * [in] Number of bytes requested by the client code.
   * @return New block size. */
  static size_t block_size(size_t number_of_blocks, size_t n_bytes_requested) {
    size_t block_size_hint;
    if (number_of_blocks < ALLOCATOR_MAX_BLOCK_MB_EXP) {
      block_size_hint = (1ULL << number_of_blocks) * 1_MiB;
    } else {
      block_size_hint = ALLOCATOR_MAX_BLOCK_BYTES;
    }
    return std::max(block_size_hint, Block::size_hint(n_bytes_requested));
  }
};

/* This is a concrete allocation scheme which is going to be default one for
 * TempTable allocator.
 *
 * It uses exponential growth policy and policy which prefers RAM allocations
 * over MMAP allocations.
 */
using Exponential_growth_preferring_RAM_over_MMAP =
    Allocation_scheme<Exponential_policy,
                      Prefer_RAM_over_MMAP_policy_obeying_per_table_limit>;

/**
  Shared state between all instances of a given allocator.

  STL allocators can (since C++11) carry state; however, that state should
  never be mutable, as the allocator can be copy-constructed and rebound
  without further notice, so e.g. deallocating memory in one allocator could
  mean freeing a block that an earlier copy of the allocator still thinks is
  valid.

  Usually, mutable state will be external to the allocator (e.g.
  Mem_root_allocator will point to a MEM_ROOT, but it won't own the MEM_ROOT);
  however, TempTable was never written this way, and doesn't have a natural
  place to stick the allocator state. Thus, we need a kludge where the
  allocator's state is held in a shared_ptr, owned by all the instances
  together. This is suboptimal for performance, and also is against the style
  guide's recommendation to have clear ownership of objects, but at least it
  avoids the use-after-free.
 */
struct AllocatorState {
  /** Current not-yet-full block to feed allocations from. */
  Block current_block;

  /**
   * Number of created blocks so far (by this Allocator object).
   * We use this number only as a hint as to how big block to create when a
   * new block needs to be created.
   */
  size_t number_of_blocks = 0;
};

/** Custom memory allocator. All dynamic memory used by the TempTable engine
 * is allocated through this allocator.
 *
 * The purpose of this allocator is to minimize the number of calls to the OS
 * for allocating new memory (e.g. malloc()) and to improve the spatial
 * locality of reference. It is able to do so quite easily thanks to the
 * Block/Chunk entities it is implemented in terms of. Due to the design of
 * these entities, it is also able to feed allocations and deallocations in
 * (amortized) constant-time and keep being CPU memory-access friendly because
 * of the internal self-adjustment to word-size memory alignment. To learn even
 * more about specifics and more properties please have a look at the respective
 * header files of Header/Block/Chunk class declarations.
 *
 * The most common use case, for which it is optimized,
 * is to have the following performed by a single thread:
 * - allocate many times (creation of a temp table and inserting data into it).
 * - use the allocated memory (selects on the temp table).
 * - free all the pieces (drop of the temp table).
 *
 * The allocator allocates memory from the OS in large blocks (e.g. a few MiB)
 * whose size also increases progressively by the increasing number of
 * allocation requests. Exact block-size increase progress is defined by the
 * block allocation scheme which, by default, is set to
 * AllocationScheme::Exponential.
 *
 * Allocator does not store a list of all allocated blocks but only keeps track
 * of the current block which has not yet been entirely filled up and the
 * overall number of allocated blocks. When current block gets filled up, new
 * one is created and immediately made current.
 *
 * Furthermore, it always keeps the last block alive. It cannot be deallocated
 * by the user. Last block is automatically deallocated at the thread exit.
 *
 * Allocator will also keep track of RAM-consumption and in case it reaches the
 * threshold defined by temptable_max_ram, it will switch to MMAP-backed block
 * allocations. It will switch back once RAM consumption is again below the
 * threshold. */
template <class T,
          class AllocationScheme = Exponential_growth_preferring_RAM_over_MMAP>
class Allocator {
  static_assert(alignof(T) <= Block::ALIGN_TO,
                "T's with alignment-requirement larger than "
                "Block::ALIGN_TO are not supported.");
  static_assert(sizeof(T) > 0, "Zero sized objects are not supported");

 public:
  typedef T *pointer;
  typedef const T *const_pointer;
  typedef T &reference;
  typedef const T &const_reference;
  typedef T value_type;
  typedef size_t size_type;
  typedef ptrdiff_t difference_type;

  template <class U>
  struct rebind {
    typedef Allocator<U, AllocationScheme> other;
  };

  /** Constructor. */
  Allocator(Block *shared_block, TableResourceMonitor &table_resource_monitor);

  /** Constructor from allocator of another type. The state is copied into the
   * new object. */
  template <class U>
  Allocator(
      /** [in] Source Allocator object. */
      const Allocator<U> &other);

  /** Move constructor from allocator of another type. */
  template <class U>
  Allocator(
      /** [in,out] Source Allocator object. */
      Allocator<U> &&other) noexcept;

  /** Destructor. */
  ~Allocator();

  Allocator(const Allocator &) = default;

  /** Assignment operator, not used, thus disabled. */
  template <class U>
  void operator=(const Allocator<U> &) = delete;

  /** Move operator, not used, thus disabled. */
  template <class U>
  void operator=(const Allocator<U> &&) = delete;

  /** Equality operator.
   * @return true if equal */
  template <class U>
  bool operator==(
      /** [in] Object to compare with. */
      const Allocator<U> &rhs) const;

  /** Inequality operator.
   * @return true if not equal */
  template <class U>
  bool operator!=(
      /** [in] Object to compare with. */
      const Allocator<U> &rhs) const;

  /** Allocate memory for storing `n_elements` number of elements. */
  T *allocate(
      /** [in] Number of elements that must be allocated. */
      size_t n_elements);

  /** Free a memory allocated by allocate(). */
  void deallocate(
      /** [in,out] Pointer to memory to free. */
      T *ptr,
      /** [in] Number of elements allocated. */
      size_t n_elements);

  /** Construct one object of type `U` on an already allocated chunk of memory,
   * which must be large enough to store it. */
  template <class U, class... Args>
  void construct(
      /** [in] Memory where to create the object. */
      U *mem,
      /** Arguments to pass to U's constructor. */
      Args &&... args);

  /** Destroy an object of type `U`. The memory is not returned to the OS, this
   * is the counterpart of `construct()`. */
  template <class U>
  void destroy(
      /** [in, out] Object to destroy. */
      U *p);

  /** Initialize necessary structures. Called once in the OS process lifetime,
   * before other methods. */
  static void init();

  /**
    Shared state between all the copies and rebinds of this allocator.
    See AllocatorState for details.
   */
  std::shared_ptr<AllocatorState> m_state;

  /** A block of memory which is a state external to this allocator and can be
   * shared among different instances of the allocator (not simultaneously). In
   * order to speed up its operations, allocator may decide to consume the
   * memory of this shared block.
   */
  Block *m_shared_block;
  /** Table resource monitor control mechanism that limits the amount of
   *  resources that can be consumed at the per-table level.
   */
  TableResourceMonitor &m_table_resource_monitor;
};

/* Implementation of inlined methods. */

template <class T, class AllocationScheme>
inline Allocator<T, AllocationScheme>::Allocator(
    Block *shared_block, TableResourceMonitor &table_resource_monitor)
    : m_state(std::make_shared<AllocatorState>()),
      m_shared_block(shared_block),
      m_table_resource_monitor(table_resource_monitor) {}

template <class T, class AllocationScheme>
template <class U>
inline Allocator<T, AllocationScheme>::Allocator(const Allocator<U> &other)
    : m_state(other.m_state),
      m_shared_block(other.m_shared_block),
      m_table_resource_monitor(other.m_table_resource_monitor) {}

template <class T, class AllocationScheme>
template <class U>
inline Allocator<T, AllocationScheme>::Allocator(Allocator<U> &&other) noexcept
    : m_state(std::move(other.m_state)),
      m_shared_block(other.m_shared_block),
      m_table_resource_monitor(other.m_table_resource_monitor) {}

template <class T, class AllocationScheme>
inline Allocator<T, AllocationScheme>::~Allocator() = default;

template <class T, class AllocationScheme>
template <class U>
inline bool Allocator<T, AllocationScheme>::operator==(
    const Allocator<U> &) const {
  return true;
}

template <class T, class AllocationScheme>
template <class U>
inline bool Allocator<T, AllocationScheme>::operator!=(
    const Allocator<U> &rhs) const {
  return !(*this == rhs);
}

template <class T, class AllocationScheme>
inline T *Allocator<T, AllocationScheme>::allocate(size_t n_elements) {
  assert(n_elements <= std::numeric_limits<size_type>::max() / sizeof(T));
  DBUG_EXECUTE_IF("temptable_allocator_oom", throw Result::OUT_OF_MEM;);
  DBUG_EXECUTE_IF("temptable_allocator_record_file_full",
                  throw Result::RECORD_FILE_FULL;);

  const size_t n_bytes_requested = n_elements * sizeof(T);
  if (n_bytes_requested == 0) {
    return nullptr;
  }

  Block *block;

  if (m_shared_block && m_shared_block->is_empty()) {
    const size_t block_size =
        AllocationScheme::block_size(0, n_bytes_requested);
    *m_shared_block = Block(
        block_size,
        AllocationScheme::block_source(block_size, &m_table_resource_monitor));
    block = m_shared_block;
  } else if (m_shared_block &&
             m_shared_block->can_accommodate(n_bytes_requested)) {
    block = m_shared_block;
  } else if (m_state->current_block.is_empty() ||
             !m_state->current_block.can_accommodate(n_bytes_requested)) {
    const size_t block_size = AllocationScheme::block_size(
        m_state->number_of_blocks, n_bytes_requested);
    m_state->current_block = Block(
        block_size,
        AllocationScheme::block_source(block_size, &m_table_resource_monitor));
    block = &m_state->current_block;
    ++m_state->number_of_blocks;
  } else {
    block = &m_state->current_block;
  }

  m_table_resource_monitor.increase(n_bytes_requested);

  T *chunk_data =
      reinterpret_cast<T *>(block->allocate(n_bytes_requested).data());
  assert(reinterpret_cast<uintptr_t>(chunk_data) % alignof(T) == 0);
  return chunk_data;
}

template <class T, class AllocationScheme>
inline void Allocator<T, AllocationScheme>::deallocate(T *chunk_data,
                                                       size_t n_elements) {
  assert(reinterpret_cast<uintptr_t>(chunk_data) % alignof(T) == 0);

  if (chunk_data == nullptr) {
    return;
  }

  const size_t n_bytes_requested = n_elements * sizeof(T);

  Block block = Block(Chunk(chunk_data));
  const auto remaining_chunks =
      block.deallocate(Chunk(chunk_data), n_bytes_requested);
  if (remaining_chunks == 0) {
    if (m_shared_block && (block == *m_shared_block)) {
      // Do nothing. Keep the last block alive.
    } else {
      assert(m_state->number_of_blocks > 0);
      if (block.type() == Source::RAM) {
        MemoryMonitor::RAM::decrease(block.size());
      } else {
        MemoryMonitor::MMAP::decrease(block.size());
      }
      if (block == m_state->current_block) {
        m_state->current_block.destroy();
      } else {
        block.destroy();
      }
      --m_state->number_of_blocks;
    }
  }
  m_table_resource_monitor.decrease(n_bytes_requested);
}

template <class T, class AllocationScheme>
template <class U, class... Args>
inline void Allocator<T, AllocationScheme>::construct(U *mem, Args &&... args) {
  new (mem) U(std::forward<Args>(args)...);
}

template <class T, class AllocationScheme>
template <class U>
inline void Allocator<T, AllocationScheme>::destroy(U *p) {
  p->~U();
}

template <class T, class AllocationScheme>
inline void Allocator<T, AllocationScheme>::init() {
  Block_PSI_init();
}

} /* namespace temptable */

#endif /* TEMPTABLE_ALLOCATOR_H */
