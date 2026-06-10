/*
  Copyright (c) 2025, 2026, Oracle and/or its affiliates.

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
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "secure_memory_pool.h"  // NOLINT(build/include_subdir)

#ifdef _WIN32
#include <Windows.h>

#include <Memoryapi.h>
#else  // !_WIN32
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <unistd.h>
#endif  // !_WIN32

#include <algorithm>
#include <bit>
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <new>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include "my_aligned_malloc.h"  // NOLINT(build/include_subdir)
#include "my_config.h"          // NOLINT(build/include_subdir)

#ifdef HAVE_VALGRIND
#include <valgrind/memcheck.h>
#include <valgrind/valgrind.h>
#endif  // HAVE_VALGRIND

namespace mysql_harness {

namespace {

constexpr std::size_t kBlockSize = 8;
// a single block must be able to hold a memory address
static_assert(sizeof(void *) <= kBlockSize);

/**
 * Size of the system page.
 */
[[nodiscard]] inline std::size_t system_page_size() {
#ifdef _WIN32
  static std::size_t page_size = []() {
    SYSTEM_INFO info;
    GetSystemInfo(&info);
    return info.dwPageSize;
  }();
#else   // !_WIN32
  static std::size_t page_size = sysconf(_SC_PAGESIZE);
#endif  // !_WIN32

  return page_size;
}

/**
 * The maximum size of memory which cannot be swapped.
 */
[[nodiscard]] inline std::size_t secure_memory_limit() {
#ifdef _WIN32
  static std::size_t memory_limit = []() {
    // The maximum number of pages that a process can lock is equal to the
    // number of pages in its minimum working set minus a small overhead.
    SIZE_T minimum;
    [[maybe_unused]] SIZE_T maximum;
    GetProcessWorkingSetSize(GetCurrentProcess(), &minimum, &maximum);
    const auto page_size = system_page_size();
    // round down to the page size
    return (minimum - 1) / page_size * page_size;
  }();
#else   // !_WIN32
  static std::size_t memory_limit = []() {
    struct rlimit rlim;
    getrlimit(RLIMIT_MEMLOCK, &rlim);
    return rlim.rlim_cur;
  }();
#endif  // !_WIN32

  return memory_limit;
}

/**
 * Allocates memory which cannot be swapped.
 */
inline void *allocate_secure_memory(std::size_t count) {
  const auto ptr = my_aligned_malloc(count, system_page_size());

  if (!ptr) [[unlikely]] {
    throw std::bad_alloc{};
  }

#ifdef _WIN32
  if (!VirtualLock(ptr, count)) {
    my_aligned_free(ptr);

    const auto code = GetLastError();
    throw std::system_error{
        std::error_code{static_cast<int>(code), std::system_category()},
        "Failed to lock memory using VirtualLock(), error code: " +
            std::to_string(code)};
  }
#else   // !_WIN32
  if (0 != mlock(ptr, count)) {
    my_aligned_free(ptr);

    const auto code = errno;
    throw std::system_error{
        std::error_code{code, std::system_category()},
        "Failed to lock memory using mlock(), errno: " + std::to_string(code)};
  }
#endif  // !_WIN32

#ifdef HAVE_VALGRIND
  VALGRIND_MAKE_MEM_NOACCESS(ptr, count);
#endif  // HAVE_VALGRIND

  return ptr;
}

/**
 * Frees memory allocated by allocate_secure_memory().
 */
inline void free_secure_memory(void *ptr, std::size_t count) noexcept {
  if (!ptr) [[unlikely]] {
    return;
  }

#ifdef _WIN32
  VirtualUnlock(ptr, count);
#else   // !_WIN32
  munlock(ptr, count);
#endif  // !_WIN32

  my_aligned_free(ptr);
}

inline void *allocate_memory(std::size_t count) {
  const auto ptr = std::malloc(count);

  if (!ptr) [[unlikely]] {
    throw std::bad_alloc{};
  }

  return ptr;
}

inline void free_memory(void *ptr) noexcept { std::free(ptr); }

/**
 * Rounds up the quotient of division of dividend by divisor.
 */
constexpr inline std::size_t round_up(std::size_t dividend,
                                      std::size_t divisor) {
  return 1 + (dividend - 1) / divisor;
}

}  // namespace

SecureMemoryPool::SecureMemoryPool()
    : large_pool_{system_page_size(), kBlockSize} {
  // the configuration is: a fixed number of buckets that serve fixed memory
  // blocks, if requested memory is bigger than what fixed buckets can serve (or
  // if the corresponding fixed bucket is full), a growing pool of buckets is
  // used to handle the request
  const auto page_size = system_page_size();
  const auto max_secure_pages = secure_memory_limit() / page_size;
  // we dedicate half of the available memory for fixed buckets
  const auto fixed_buckets = std::min<std::size_t>(max_secure_pages / 2, 8);

  fixed_buckets_mutexes_ = std::vector<std::mutex>{fixed_buckets};

  for (std::size_t i = 1; i <= fixed_buckets; ++i) {
    fixed_buckets_.emplace_back(page_size, i * kBlockSize);
  }
}

[[nodiscard]] SecureMemoryPool &SecureMemoryPool::get() {
  static SecureMemoryPool instance;
  return instance;
}

[[nodiscard]] void *SecureMemoryPool::allocate(std::size_t size) {
  assert(size);

  if (const auto index = (size - 1) / kBlockSize;
      index < fixed_buckets_.size()) {
    std::lock_guard lock{fixed_buckets_mutexes_[index]};

    if (const auto ptr = fixed_buckets_[index].allocate(size)) {
      return ptr;
    }
  }

  return large_pool_.allocate(size);
}

void SecureMemoryPool::deallocate(void *ptr, std::size_t size) noexcept {
  if (!ptr) [[unlikely]] {
    return;
  }

  assert(size);

  if (const auto index = (size - 1) / kBlockSize;
      index < fixed_buckets_.size()) {
    std::lock_guard lock{fixed_buckets_mutexes_[index]};

    if (fixed_buckets_[index].contains(ptr)) {
      fixed_buckets_[index].deallocate(ptr, size);
      return;
    }
  }

  large_pool_.deallocate(ptr, size);
}

SecureMemoryPool::ContiguousBlocks::ContiguousBlocks(
    const Bucket<ContiguousBlocks> &parent)
    : parent_(parent) {
  // size of the index is rounded up
  const auto index_size = round_up(parent_.block_count(), 8);

  index_ = static_cast<unsigned char *>(allocate_memory(index_size));
  std::memset(index_, 0, index_size);
}

SecureMemoryPool::ContiguousBlocks::~ContiguousBlocks() { free_memory(index_); }

[[nodiscard]] void *SecureMemoryPool::ContiguousBlocks::allocate_blocks(
    std::size_t count) noexcept {
  // find the given number of contiguous blocks
  const auto index = find_contiguous_blocks(count);

  if (parent_.block_count() == index) {
    // not enough contiguous blocks
    return nullptr;
  }

  set_in_use(index, count);

  return parent_.memory() + (index * parent_.block_size());
}

void SecureMemoryPool::ContiguousBlocks::deallocate_blocks(
    void *ptr, std::size_t count) noexcept {
  const auto p = static_cast<std::byte *>(ptr);
  const auto index =
      static_cast<std::size_t>(p - parent_.memory()) / parent_.block_size();

  set_free(index, count);
}

/**
 * Finds the given number of contiguous blocks.
 */
[[nodiscard]] std::size_t
SecureMemoryPool::ContiguousBlocks::find_contiguous_blocks(
    std::size_t count) const noexcept {
  assert(count);

  // current pointer to the index
  auto ptr = index_ - 1;
  // contents of the index at the current pointer
  unsigned char byte;
  // number of bits which were not checked in the current byte
  std::size_t bits_left;

  // moves to the next byte in the index
  const auto next_byte = [&]() {
    ++ptr;
    byte = *ptr;
    bits_left = 8;
  };

  // move to the first byte
  next_byte();

  // number of bits checked
  std::size_t bits;

  // consumes the given number of bits in the current byte
  const auto consume_bits = [&]() {
    byte >>= bits;
    bits_left -= bits;
  };

  // current index
  std::size_t index = 0;
  // number of consecutive unused blocks
  std::size_t run;

  const auto block_count = parent_.block_count();

  while (index < block_count) {
    // find the first unused block
    while (index < block_count) {
      bits = std::countr_one(byte);
      index += bits;

      consume_bits();

      if (bits_left) {
        // unused block was found
        break;
      } else if (index < block_count) {
        // unused block not found, move to the next byte
        next_byte();
      }
    }

    // count the number of consecutive unused blocks
    run = 0;

    while (index + run < block_count) {
      bits = std::min<std::size_t>(std::countr_zero(byte), bits_left);
      run += bits;

      if (run >= count) {
        // we have enough memory
        return index;
      }

      consume_bits();

      if (bits_left) {
        // used block was found, not enough memory
        break;
      } else if (index + run < block_count) {
        // used block not found, move to the next byte
        next_byte();
      }
    }

    index += run;
  }

  return index;
}

/**
 * Marks the given number of blocks starting at the given index as used.
 */
void SecureMemoryPool::ContiguousBlocks::set_in_use(
    std::size_t index, std::size_t count) noexcept {
  assert(count);

  unsigned char mask;
  std::size_t mask_length;

  auto ptr = index_ + index / 8;
  std::size_t bit = index % 8;

  while (count) {
    mask_length = std::min(8 - bit, count);

    mask = (1 << mask_length) - 1;
    mask <<= bit;

    *ptr++ |= mask;

    count -= mask_length;
    bit = 0;
  }
}

/**
 * Marks the given number of blocks starting at the given index as unused.
 */
void SecureMemoryPool::ContiguousBlocks::set_free(std::size_t index,
                                                  std::size_t count) noexcept {
  assert(count);

  unsigned char mask;
  std::size_t mask_length;

  auto ptr = index_ + index / 8;
  std::size_t bit = index % 8;

  while (count) {
    mask_length = std::min(8 - bit, count);

    mask = (1 << mask_length) - 1;
    mask <<= bit;
    mask = ~mask;

    *ptr++ &= mask;

    count -= mask_length;
    bit = 0;
  }
}

SecureMemoryPool::FixedBlock::FixedBlock(const Bucket<FixedBlock> &parent)
    : parent_(parent) {
  auto block_count = parent_.block_count();
  const auto block_size = parent_.block_size();

  // create a list of unused blocks, list is stored within the blocks
  // head points to the first available block

  auto ptr = parent_.memory();
  unused_blocks_ = reinterpret_cast<BlockList *>(ptr);

  BlockList *item = unused_blocks_;

  while (--block_count) {
    ptr += block_size;

#ifdef HAVE_VALGRIND
    const auto safe_ptr = &item->next;
    VALGRIND_MAKE_MEM_DEFINED(safe_ptr, sizeof(BlockList *));
#endif  // HAVE_VALGRIND

    item->next = reinterpret_cast<BlockList *>(ptr);
    item = item->next;

#ifdef HAVE_VALGRIND
    VALGRIND_MAKE_MEM_NOACCESS(safe_ptr, sizeof(BlockList *));
#endif  // HAVE_VALGRIND
  }

#ifdef HAVE_VALGRIND
  VALGRIND_MAKE_MEM_DEFINED(&item->next, sizeof(BlockList *));
#endif  // HAVE_VALGRIND

  item->next = nullptr;

#ifdef HAVE_VALGRIND
  VALGRIND_MAKE_MEM_NOACCESS(&item->next, sizeof(BlockList *));
#endif  // HAVE_VALGRIND
}

SecureMemoryPool::FixedBlock::FixedBlock(FixedBlock &&fb) noexcept
    : parent_(fb.parent_) {
  unused_blocks_ = std::exchange(fb.unused_blocks_, nullptr);
}

[[nodiscard]] void *SecureMemoryPool::FixedBlock::allocate_blocks(
    [[maybe_unused]] std::size_t count) noexcept {
  assert(1 == count);
  assert(unused_blocks_);

#ifdef HAVE_VALGRIND
  const auto safe_ptr = &unused_blocks_->next;
  VALGRIND_MAKE_MEM_DEFINED(safe_ptr, sizeof(BlockList *));
#endif  // HAVE_VALGRIND

  const auto ptr = std::exchange(unused_blocks_, unused_blocks_->next);

#ifdef HAVE_VALGRIND
  VALGRIND_MAKE_MEM_NOACCESS(safe_ptr, sizeof(BlockList *));
#endif  // HAVE_VALGRIND

  return ptr;
}

void SecureMemoryPool::FixedBlock::deallocate_blocks(
    void *ptr, [[maybe_unused]] std::size_t count) noexcept {
  assert(1 == count);

  auto block = reinterpret_cast<BlockList *>(ptr);

#ifdef HAVE_VALGRIND
  VALGRIND_MAKE_MEM_DEFINED(&block->next, sizeof(BlockList *));
#endif  // HAVE_VALGRIND

  block->next = std::exchange(unused_blocks_, block);

#ifdef HAVE_VALGRIND
  VALGRIND_MAKE_MEM_NOACCESS(&block->next, sizeof(BlockList *));
#endif  // HAVE_VALGRIND
}

template <class Strategy>
SecureMemoryPool::Bucket<Strategy>::Bucket(std::size_t bucket_size,
                                           std::size_t block_size)
    : bucket_size_(bucket_size),
      block_count_(bucket_size / block_size),
      block_size_(block_size),
      blocks_free_(block_count_),
      memory_(static_cast<std::byte *>(allocate_secure_memory(bucket_size_))),
      allocator_(*this) {
  assert(0 == bucket_size_ % system_page_size());
  assert(block_count_ && block_size_);
}

template <class Strategy>
SecureMemoryPool::Bucket<Strategy>::Bucket(Bucket &&b) noexcept
    : bucket_size_(b.bucket_size_),
      block_count_(b.block_count_),
      block_size_(b.block_size_),
      blocks_free_(b.blocks_free_),
      allocator_(std::move(b.allocator_)) {
  memory_ = std::exchange(b.memory_, nullptr);
}

template <class Strategy>
SecureMemoryPool::Bucket<Strategy>::~Bucket() {
  free_secure_memory(memory_, bucket_size());
}

template <class Strategy>
[[nodiscard]] bool SecureMemoryPool::Bucket<Strategy>::contains(
    const void *ptr) const noexcept {
  return ptr >= memory_ && ptr < memory_ + bucket_size();
}

template <class Strategy>
[[nodiscard]] void *SecureMemoryPool::Bucket<Strategy>::allocate(
    std::size_t bytes) noexcept {
  assert(bytes);

  // number of blocks needed
  const auto count = round_up(bytes, block_size_);

  if (blocks_free_ < count) {
    // not enough blocks in the bucket
    return nullptr;
  }

  // allocate blocks
  const auto ptr = allocator_.allocate_blocks(count);

  if (ptr) {
#ifdef HAVE_VALGRIND
    VALGRIND_MAKE_MEM_UNDEFINED(ptr, bytes);
#endif  // HAVE_VALGRIND

    blocks_free_ -= count;
  }

  return ptr;
}

template <class Strategy>
void SecureMemoryPool::Bucket<Strategy>::deallocate(
    void *ptr, std::size_t bytes) noexcept {
  assert(ptr && bytes);
  assert(contains(ptr));

#ifdef HAVE_VALGRIND
  VALGRIND_MAKE_MEM_NOACCESS(ptr, bytes);
#endif  // HAVE_VALGRIND

  // number of blocks used
  const auto count = round_up(bytes, block_size_);

  allocator_.deallocate_blocks(ptr, count);
  blocks_free_ += count;
}

template <class BucketType>
SecureMemoryPool::BucketPool<BucketType>::BucketPool(std::size_t page_size,
                                                     std::size_t block_size)
    : page_size_(page_size),
      block_size_(block_size),
      empty_bucket_(add_bucket(page_size_)) {}

template <class BucketType>
[[nodiscard]] void *SecureMemoryPool::BucketPool<BucketType>::allocate(
    std::size_t size) {
  // pointer to the allocated memory
  void *ptr;

  // tries to allocate the memory in the given bucket
  const auto maybe_allocate = [&, this](BucketType *bucket) {
    ptr = bucket->allocate(size);

    if (!ptr) {
      return;
    }

    if (empty_bucket_ == bucket) {
      // bucket is no longer empty
      empty_bucket_ = nullptr;
    }

    if (bucket->is_full()) {
      // bucket is full, move it to the other container
      full_buckets_.insert(buckets_.extract(*bucket));
    }
  };

  std::lock_guard lock{mutex_};

  // search through the buckets which have some memory available
  for (auto it = buckets_.begin(), end = buckets_.end(); it != end; ++it) {
    maybe_allocate(pointer(it));

    if (ptr) {
      return ptr;
    }
  }

  // add a new bucket, allocate the memory there
  maybe_allocate(add_bucket(size));

  assert(ptr);

  return ptr;
}

template <class BucketType>
void SecureMemoryPool::BucketPool<BucketType>::deallocate(
    void *ptr, std::size_t size) noexcept {
  std::lock_guard lock{mutex_};

  auto bucket = find_bucket(ptr);

  if (bucket->is_full()) {
    // bucket is no longer going to be full, move it to the other container
    buckets_.insert(full_buckets_.extract(*bucket));
  }

  bucket->deallocate(ptr, size);

  if (bucket->is_empty()) {
    if (empty_bucket_) {
      // we already have an empty bucket, release this one
      remove_bucket(bucket);
    } else {
      // keep the empty bucket for now
      empty_bucket_ = bucket;
    }
  }
}

template <class BucketType>
[[nodiscard]] BucketType *SecureMemoryPool::BucketPool<BucketType>::add_bucket(
    std::size_t size) {
  // round up to the page size
  if (const std::size_t pages = size / page_size_,
      new_size = pages * page_size_;
      new_size != size) {
    assert(new_size + page_size_ > size);
    size = new_size + page_size_;
  }

  auto bucket = pointer(buckets_.emplace(size, block_size_).first);

  // each bucket holds memory aligned to the system page size
  // size of that memory is a factor of the system page size
  // we put address of each page held by the bucket into the map
  const auto page_size = system_page_size();

  for (auto ptr = bucket->memory(); bucket->contains(ptr); ptr += page_size) {
    memory_map_.emplace(ptr, bucket);
  }

  return bucket;
}

template <class BucketType>
void SecureMemoryPool::BucketPool<BucketType>::remove_bucket(
    BucketType *bucket) {
  const auto page_size = system_page_size();

  for (auto ptr = bucket->memory(); bucket->contains(ptr); ptr += page_size) {
    memory_map_.erase(ptr);
  }

  buckets_.erase(*bucket);
}

/**
 * Finds the bucket which holds the given memory address.
 */
template <class BucketType>
[[nodiscard]] BucketType *SecureMemoryPool::BucketPool<BucketType>::find_bucket(
    const void *ptr) {
  // round down the address to the system page size, find the bucket based on
  // that page
  const auto mask = ~(system_page_size() - 1);
  const auto it = memory_map_.find(
      reinterpret_cast<void *>(reinterpret_cast<std::size_t>(ptr) & mask));

  assert(memory_map_.end() != it);

  return it->second;
}

}  // namespace mysql_harness
