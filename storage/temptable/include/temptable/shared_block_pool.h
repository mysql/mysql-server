/* Copyright (c) 2020, 2023, Oracle and/or its affiliates.

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

/** @file storage/temptable/include/temptable/shared_block_pool.h
TempTable shared-block pool implementation. */

#ifndef TEMPTABLE_SHARED_BLOCK_POOL_H
#define TEMPTABLE_SHARED_BLOCK_POOL_H

#include <array>
#include <cstddef>
#include <limits>

#include "storage/temptable/include/temptable/block.h"
#include "storage/temptable/include/temptable/constants.h"
#include "storage/temptable/include/temptable/lock_free_pool.h"
#include "storage/temptable/include/temptable/lock_free_type.h"

namespace temptable {

/** Lock-free pool of POOL_SIZE Block elements.
 *
 * Each Block element in a pool is represented by a slot. Each slot can be
 * either free (default) or occupied. Acquiring the Block is possible only from
 * an empty slot. Releasing the Block makes the slot free again.
 *
 * Acquiring and releasing the Block is done via THD identifier on which,
 * implicitly, modulo-arithmethic is applied in order to pick the right slot.
 * To avoid questionable performance of modulo-arithmetic in its more general
 * form, POOL_SIZE is constrained only to numbers which are power of two. This
 * enables us to implement a modulo operation in single bitwise instruction.
 * Check whether POOL_SIZE is a number which is power of two is run during the
 * compile-time.
 *
 * Given that this pool is envisioned to be used concurrently by multiple
 * threads, both the slot and Block are aligned to the size of CPU L1
 * data-cache. This makes sure that we negate the effect of false-sharing
 * (threads bouncing each ones data from cache).
 */
template <size_t POOL_SIZE>
class Lock_free_shared_block_pool {
  /** Check whether all compile-time pre-conditions are set. */
  static_assert(POOL_SIZE && !(POOL_SIZE & (POOL_SIZE - 1)),
                "POOL_SIZE template parameter must be a power of two.");

  /** Build a slot type:
   *    a lock-free pool of L1-DCACHE aligned unsigned long long's
   */
  using Shared_block_slot =
      Lock_free_pool<unsigned long long, POOL_SIZE, Alignment::L1_DCACHE_SIZE>;

  /** Be pedantic about the element type we used when building the slot type. */
  using Shared_block_slot_element_type = typename Shared_block_slot::Type;

  /** Constexpr variable denoting a non-occupied (free) slot. */
  static constexpr Shared_block_slot_element_type FREE_SLOT =
      std::numeric_limits<Shared_block_slot_element_type>::max();

  /** Bitmask which enables us to implement modulo-arithmetic operation in
   * single bitwise instruction. */
  static constexpr size_t MODULO_MASK = POOL_SIZE - 1;

  /** In the event of inability to express ourselves with something like
   * std::array<alignas<N> Block> we have to fallback to this method.
   * */
  struct L1_dcache_aligned_block {
    alignas(L1_DCACHE_SIZE) Block block;
  };

  /** An array of L1-dcache aligned Blocks
   *  Note: This is _not_ the same as:
   *    alignas(L1_DCACHE_SIZE) std::array<Block, POOL_SIZE>
   * */
  std::array<L1_dcache_aligned_block, POOL_SIZE> m_shared_block;

  /** Lock-free slots. */
  Shared_block_slot m_slot{FREE_SLOT};

 public:
  /** Given the THD identifier, try to acquire an instance of Block. In the
   * event of success, non-nullptr instance of Block will be returned and
   * underlying slot will be marked as occupied. Otherwise, slot must have been
   * already occupied in which case a nullptr will be returned.
   *
   * Using the same THD identifier to re-acquire the Block (from the same slot)
   * is supported but not possible if some other THD identifier is used.
   *
   * [in] thd_id THD identifier.
   * @return Returns a pointer to the Block (success) or nullptr (failure).
   * */
  Block *try_acquire(size_t thd_id) {
    auto slot_idx = thd_id & MODULO_MASK;
    auto slot_thd_id = FREE_SLOT;
    if (m_slot.compare_exchange_strong(slot_idx, slot_thd_id, thd_id)) {
      return &m_shared_block[slot_idx].block;
    } else if (slot_thd_id == thd_id) {
      return &m_shared_block[slot_idx].block;
    } else {
      return nullptr;
    }
  }

  /** Given the THD identifier, try to release an acquired instance of a
   * Block. In the event of success, slot will be marked as non-occupied.
   * Assuming that Block is not empty, it will also be destroyed.
   *
   * Trying to release the Block/slot by using some other THD identifier is not
   * possible and will therefore render this operation as failed.
   *
   * [in] thd_id THD identifier.
   * @return Returns true (success) or false (failure).
   * */
  bool try_release(size_t thd_id) {
    auto slot_idx = thd_id & MODULO_MASK;
    if (m_slot.load(slot_idx) == thd_id) {
      auto &block = m_shared_block[slot_idx].block;
      if (!block.is_empty()) {
        if (block.type() == Source::RAM) {
          MemoryMonitor::RAM::decrease(block.size());
        } else if (block.type() == Source::MMAP_FILE) {
          MemoryMonitor::MMAP::decrease(block.size());
        }
        block.destroy();
      }
      m_slot.store(slot_idx, FREE_SLOT);
      return true;
    }
    return false;
  }
};

}  // namespace temptable

#endif /* TEMPTABLE_SHARED_BLOCK_POOL_H */
