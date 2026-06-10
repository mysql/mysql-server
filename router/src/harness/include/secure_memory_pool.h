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

#ifndef ROUTER_SRC_HARNESS_INCLUDE_SECURE_MEMORY_POOL_H_
#define ROUTER_SRC_HARNESS_INCLUDE_SECURE_MEMORY_POOL_H_

#include <cstddef>
#include <list>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "harness_export.h"  // NOLINT(build/include_subdir)

namespace mysql_harness {

/**
 * Manages a pool of memory which is prevented from being swapped.
 */
class HARNESS_EXPORT SecureMemoryPool final {
 public:
  SecureMemoryPool(const SecureMemoryPool &) = delete;
  SecureMemoryPool(SecureMemoryPool &&) = delete;

  SecureMemoryPool &operator=(const SecureMemoryPool &) = delete;
  SecureMemoryPool &operator=(SecureMemoryPool &&) = delete;

  ~SecureMemoryPool() = default;

  /**
   * The single instance of this class.
   */
  [[nodiscard]] static SecureMemoryPool &get();

  /**
   * Allocates the given number of bytes.
   *
   * @param size Number of bytes to allocate.
   *
   * @returns Allocated memory.
   */
  [[nodiscard]] void *allocate(std::size_t size);

  /**
   * Deallocates the given number of bytes.
   *
   * @param ptr Memory previously allocated by this class.
   * @param size Size of the memory.
   */
  void deallocate(void *ptr, std::size_t size) noexcept;

 private:
  template <class Strategy>
  class Bucket;

  /**
   * Allows to allocate multiple contiguous blocks of memory.
   */
  class ContiguousBlocks final {
   public:
    ContiguousBlocks() = delete;

    explicit ContiguousBlocks(const Bucket<ContiguousBlocks> &parent);

    ContiguousBlocks(const ContiguousBlocks &) = delete;
    ContiguousBlocks(ContiguousBlocks &&) = delete;

    ContiguousBlocks &operator=(const ContiguousBlocks &) = delete;
    ContiguousBlocks &operator=(ContiguousBlocks &&) = delete;

    ~ContiguousBlocks();

    /**
     * Allocates the given number of blocks.
     *
     * @param count Number of blocks to allocate.
     *
     * @returns Allocated memory or nullptr if cannot allocate the requested
     *          amount of blocks.
     */
    [[nodiscard]] void *allocate_blocks(std::size_t count) noexcept;

    /**
     * Deallocates the given number of blocks.
     *
     * @param ptr Memory previously allocated by this class.
     * @param count Number of blocks.
     */
    void deallocate_blocks(void *ptr, std::size_t count) noexcept;

   private:
    [[nodiscard]] std::size_t find_contiguous_blocks(
        std::size_t count) const noexcept;

    void set_in_use(std::size_t index, std::size_t count) noexcept;

    void set_free(std::size_t index, std::size_t count) noexcept;

    const Bucket<ContiguousBlocks> &parent_;
    // holds one bit per each block of memory, 0 block is unused, 1 block is in
    // use
    unsigned char *index_;
  };

  /**
   * Allocates a single block of memory.
   */
  class FixedBlock final {
   public:
    FixedBlock() = delete;

    explicit FixedBlock(const Bucket<FixedBlock> &parent);

    FixedBlock(const FixedBlock &) = delete;
    FixedBlock(FixedBlock &&) noexcept;

    FixedBlock &operator=(const FixedBlock &) = delete;
    FixedBlock &operator=(FixedBlock &&) = delete;

    ~FixedBlock() = default;

    /**
     * Allocates the given number of blocks.
     *
     * @param count Number of blocks to allocate.
     *
     * @returns Allocated memory or nullptr if cannot allocate the requested
     *          amount of blocks.
     */
    [[nodiscard]] void *allocate_blocks(std::size_t count) noexcept;

    /**
     * Deallocates the given number of blocks.
     *
     * @param ptr Memory previously allocated by this class.
     * @param count Number of blocks.
     */
    void deallocate_blocks(void *ptr, std::size_t count) noexcept;

   private:
    struct BlockList {
      BlockList *next;
    };

    const Bucket<FixedBlock> &parent_;
    // unused blocks
    BlockList *unused_blocks_;
  };

  /**
   * A bucket of memory blocks of the given size.
   */
  template <class Strategy>
  class Bucket final {
   public:
    Bucket() = delete;

    /**
     * Creates the bucket.
     *
     * @param bucket_size Size of the whole bucket.
     * @param block_size Size of a single block of memory.
     */
    Bucket(std::size_t bucket_size, std::size_t block_size);

    Bucket(const Bucket &) = delete;
    Bucket(Bucket &&) noexcept;

    Bucket &operator=(const Bucket &) = delete;
    Bucket &operator=(Bucket &&) = delete;

    ~Bucket();

    /**
     * Size of the whole bucket.
     */
    [[nodiscard]] inline std::size_t bucket_size() const noexcept {
      return bucket_size_;
    }

    /**
     * Number of memory blocks in this bucket.
     */
    [[nodiscard]] inline std::size_t block_count() const noexcept {
      return block_count_;
    }

    /**
     * Size of a single block of memory.
     */
    [[nodiscard]] inline std::size_t block_size() const noexcept {
      return block_size_;
    }

    /**
     * Number of unused blocks.
     */
    [[nodiscard]] inline std::size_t blocks_free() const noexcept {
      return blocks_free_;
    }

    /**
     * Whether all blocks are unused.
     */
    [[nodiscard]] inline std::size_t is_empty() const noexcept {
      return block_count() == blocks_free();
    }

    /**
     * Whether all blocks are being used.
     */
    [[nodiscard]] inline std::size_t is_full() const noexcept {
      return 0 == blocks_free();
    }

    /**
     * Pointer to the memory held by this bucket.
     */
    [[nodiscard]] inline std::byte *memory() const noexcept { return memory_; }

    /**
     * Whether this bucket contains the given pointer.
     */
    [[nodiscard]] bool contains(const void *ptr) const noexcept;

    /**
     * Allocates the given number of bytes.
     *
     * @param bytes Number of bytes to allocate.
     *
     * @returns Allocated memory or nullptr if bucket cannot allocate the
     *          requested amount of memory.
     */
    [[nodiscard]] void *allocate(std::size_t bytes) noexcept;

    /**
     * Deallocates the given number of bytes.
     *
     * @param ptr Memory previously allocated by this class.
     * @param bytes Size of the memory.
     */
    void deallocate(void *ptr, std::size_t bytes) noexcept;

    /**
     * Compares two buckets.
     */
    friend inline bool operator==(const Bucket &l, const Bucket &r) noexcept {
      return l.memory_ == r.memory_;
    }

   private:
    const std::size_t bucket_size_;
    const std::size_t block_count_;
    const std::size_t block_size_;
    std::size_t blocks_free_;

    // allocated memory
    std::byte *memory_;
    // allocator strategy to use
    Strategy allocator_;
  };

  /**
   * Holds buckets with the given block size.
   */
  template <class BucketType>
  class BucketPool final {
   public:
    BucketPool() = delete;

    /**
     * Creates the pool.
     *
     * @param page_size Page size for the buckets in this pool, size of each
     *                  bucket is going to be a factor of this size.
     * @param block_size Blocks size for the buckets in this pool.
     */
    BucketPool(std::size_t page_size, std::size_t block_size);

    BucketPool(const BucketPool &) = delete;
    BucketPool(BucketPool &&) = delete;

    BucketPool &operator=(const BucketPool &) = delete;
    BucketPool &operator=(BucketPool &&) = delete;

    ~BucketPool() = default;

    /**
     * Page size for the buckets in this pool.
     */
    [[nodiscard]] inline std::size_t page_size() const noexcept {
      return page_size_;
    }

    /**
     * Size of a single block of memory.
     */
    [[nodiscard]] inline std::size_t block_size() const noexcept {
      return block_size_;
    }

    /**
     * Allocates the given number of bytes.
     *
     * @param bytes Number of bytes to allocate.
     *
     * @returns Allocated memory.
     */
    [[nodiscard]] void *allocate(std::size_t bytes);

    /**
     * Deallocates the given number of bytes.
     *
     * @param ptr Memory previously allocated by this class.
     * @param bytes Size of the memory.
     */
    void deallocate(void *ptr, std::size_t bytes) noexcept;

   private:
    struct BucketHash {
      std::size_t operator()(const BucketType &b) const noexcept {
        // buckets are hashed using the memory they hold
        return reinterpret_cast<std::size_t>(b.memory());
      }
    };

    using Buckets = std::unordered_set<BucketType, BucketHash>;

    [[nodiscard]] static inline BucketType *pointer(
        Buckets::const_iterator it) {
      // buckets are stored in an unordered set, where iterators are always
      // const; we can safely cast away the const, as the memory allocated by
      // each bucket does not change during their lifetime, ensuring that hash
      // does not change
      return const_cast<BucketType *>(&*it);
    }

    [[nodiscard]] BucketType *add_bucket(std::size_t size);

    void remove_bucket(BucketType *bucket);

    [[nodiscard]] BucketType *find_bucket(const void *ptr);

    const std::size_t page_size_;
    const std::size_t block_size_;

    std::mutex mutex_;

    // buckets which have memory available
    Buckets buckets_;
    // buckets which don't have memory available
    Buckets full_buckets_;

    // maps the memory aligned to the system page to the bucket which holds that
    // memory
    std::unordered_map<const void *, BucketType *> memory_map_;

    // we keep a single empty bucket to avoid repeatedly adding and removing a
    // new bucket when close to the capacity limit
    const BucketType *empty_bucket_;
  };

  SecureMemoryPool();

  // buckets which serve memory blocks of a constant size
  std::vector<Bucket<FixedBlock>> fixed_buckets_;
  // mutexes which control access to the fixed_buckets_
  std::vector<std::mutex> fixed_buckets_mutexes_;
  // pool which serves memory that does not fit into other pools
  BucketPool<Bucket<ContiguousBlocks>> large_pool_;
};

}  // namespace mysql_harness

#endif  // ROUTER_SRC_HARNESS_INCLUDE_SECURE_MEMORY_POOL_H_
