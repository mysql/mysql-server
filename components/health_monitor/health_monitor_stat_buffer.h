/* Copyright (c) 2020, 2026, Oracle and/or its affiliates.

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

#pragma once

#include <algorithm>
#include <memory>
#include <string>
#include <utility>  // std::cmp_less()
#include <vector>

#include <mysql/components/library_mysys/component_malloc_allocator.h>
#include <mysql/components/services/psi_memory.h>

#include "health_monitor_lock.h"

namespace hm {

/**
  Sample data buffer.
*/
template <typename T>
class Stat_Buffer {
  using Allocator = Component_malloc_allocator<T>;

 public:
  Stat_Buffer() = default;
  Stat_Buffer(bool no_lock, PSI_memory_key memory_key)
      : m_no_lock{no_lock}, m_memory_key{memory_key} {
    m_stats =
        std::make_unique<std::vector<T, Allocator>>((Allocator(m_memory_key)));
  }

  Stat_Buffer(PSI_rwlock_key rwlock_key, PSI_memory_key memory_key)
      : m_rwlock_key{rwlock_key}, m_memory_key{memory_key} {
    mysql_rwlock_init(m_rwlock_key, &m_rwlock);
    m_stats =
        std::make_unique<std::vector<T, Allocator>>((Allocator(m_memory_key)));
  }

  virtual ~Stat_Buffer() {
    if (!m_no_lock) {
      mysql_rwlock_destroy(&m_rwlock);
    }
  }

  Stat_Buffer(const Stat_Buffer &) = default;
  Stat_Buffer &operator=(const Stat_Buffer &) = default;
  Stat_Buffer(Stat_Buffer &&) noexcept = default;
  Stat_Buffer &operator=(Stat_Buffer &&) noexcept = default;

  /**
    Read an element of the stat buffer.
    @param[in,out] record   Element from the buffer to be returned
    @param[in]     row_num  Row number, zero-based
    @return  0 success, 1 failure
  */
  int read(T &record, unsigned int row_num) {
    if (m_no_lock) {
      return read_impl(record, row_num);
    }
    rwlock_guard lock(&m_rwlock, false, __FILE__, __LINE__);
    return read_impl(record, row_num);
  }

  /**
    Read the last element of the stat buffer.
    @param[in,out] record  Element from the buffer to be returned
    @return  0 success, 1 failure
  */
  int read_last(T &record) {
    rwlock_guard lock(&m_rwlock, false, __FILE__, __LINE__);
    if (m_count == 0) {
      return 1;
    }
    return read_impl(record, m_count - 1);
  }

  /**
    Acquire a read lock for a shared buffer.
    @return 0 success
    @return 1 fail to set lock
  */
  int lock_read() {
    if (m_no_lock) {
      return 1;
    }
    return rwlock_rdlock(&m_rwlock, __FILE__, __LINE__);
  }

  /**
    Unlock a read or write lock for a shared buffer.
  */
  void unlock() {
    if (m_no_lock) {
      return;
    }
    rwlock_unlock(&m_rwlock);
  }

  /**
    @brief Read an element from the circular buffer by logical row index.

    The buffer holds up to m_count valid elements. This function accepts a row
    non-negative row_num range [0, m_count].

    Thread safety:
    - If this buffer is shared (m_no_lock == false), the caller should hold a
      read lock or use read()/read_last() which manage locking.
    - This method itself does not acquire locks.

    @param[out] record   On success, set to element at requested logical row.
    @param[in]  row_num  Zero-based logical row index [0, m_count].
    @return 0 on success.
    @return 1 if the buffer is empty (no elements to read).
    @note Logical indexing: When the buffer has wrapped, the oldest element
          begins at m_index; otherwise it begins at 0.
  */
  int read_no_lock(T &record, unsigned int row_num) {
    return read_impl(record, row_num);
  }

  int read_impl(T &record, size_t row_num) {
    if (m_count == 0 || m_stats->empty() || row_num >= m_count) {
      return 1;  // nothing to read
    }
    const size_t base = m_wrapped ? m_index : 0;  // oldest element
    const size_t idx = (base + row_num) % m_stats->size();
    record = (*m_stats)[idx];
    return 0;
  }

  /**
    @brief Write one element into the circular buffer at the current head
    position.

    If the buffer is not yet full, the element is appended logically and the
    valid element count (m_count) increases. The head index (m_index) advances
    modulo the buffer size.

    Once the buffer reaches capacity:
    - Subsequent writes overwrite the oldest element
    - m_full remains true
    - m_wrapped is set when the first overwrite occurs and stays true

    Thread safety:
    - This method does not acquire locks. For shared buffers (m_no_lock ==
    false), call write() which manages locking, or ensure the caller holds an
    exclusive lock.

    @param[in] record Element to write.
    @return 0 on success.
    @return 1 if the buffer has zero capacity (size() == 0).
    @note Uses logical indexing consistent with read_impl(): When not wrapped,
          the oldest element is at index 0. After wrapping, the oldest begins
          at m_index.
  */
  int write(const T &record) {
    if (m_no_lock) {
      return write_impl(record);
    }
    rwlock_guard lock(&m_rwlock, true, __FILE__, __LINE__);
    return write_impl(record);
  }

  int write_impl(const T &record) {
    const size_t size = m_stats->size();
    if (size == 0) {
      return 1;  // cannot write without capacity
    }

    // Write at current head
    (*m_stats)[m_index] = record;
    m_total_written++;

    // Update count/full/wrapped
    if (m_count < size) {
      ++m_count;
      if (m_count == size) {
        m_full = true;  // just became full
      }
    } else {
      // Already full, we're overwriting the oldest so mark wrapped.
      m_wrapped = true;
    }
    // Advance head
    m_index = (m_index + 1) % size;  // next write

    return 0;
  }

  /**
    @brief Resize the stat buffer, preserve the most recent data.

    Note that m_stats is preallocated to m_max_size elements,
    so m_stats->size() is always the maximum size.
    @param[in] new_size  Desired size of the buffer, can be either
                         larger or smaller than the current size
  */
  void resize(size_t new_size) {
    if (m_no_lock) {
      return resize_impl(new_size);
    }
    rwlock_guard lock(&m_rwlock, true, __FILE__, __LINE__);
    return resize_impl(new_size);
  }

  void resize_impl(size_t new_size) {
    const size_t old_size = m_stats->size();
    size_t base = 0;  // oldest record to keep

    // Buffer is empty, resize and reset
    if (m_stats->empty() || m_count == 0 || new_size == 0) {
      if (new_size != old_size) {
        m_max_size = new_size;
        m_stats->resize(m_max_size);
      }
      m_index = 0;
      m_count = 0;
      m_full = false;
      m_wrapped = false;
      return;
    }
    // Find the range of data samples to keep. m_index points to
    // the next element to be written. If the buffer has wrapped,
    // then m_index points to the oldest element.
    if (new_size > old_size) {
      // extend the buffer, no other changes
      m_full = false;
      if (!m_wrapped) {
        m_max_size = new_size;
        m_stats->resize(m_max_size);
        return;
      }
      // unwrap into sequential order
      base = m_index;
      m_index = old_size;
    } else {
      // shrink the buffer
      if (m_index >= new_size) {
        // sample data is contiguous and exceeds the new buffer size
        base = m_index - new_size;
        m_index = 0;
        m_full = true;
      } else {
        if (m_wrapped) {
          // sample data is split
          base = old_size - new_size + m_index;
          m_index = 0;
          m_full = true;
        } else {
          // sample data is contiguous and fits within the new buffer
          base = 0;
          m_full = false;
        }
      }
    }

    // Reallocate the buffer with the most recent records
    std::vector<T, Allocator> vtemp(new_size, (Allocator(m_memory_key)));
    const size_t new_count = std::min(new_size, m_count);
    for (size_t i = 0; i < new_count; i++) {
      const size_t idx = (base + i) % m_stats->size();
      vtemp.at(i) = m_stats->at(idx);
    }
    m_stats->swap(vtemp);
    m_max_size = new_size;
    m_count = new_count;
    m_wrapped = false;
  }

  /**
    Reset the state of the buffer. Existing elements will be overwritten.
  */
  void reset() {
    if (m_no_lock) {
      return reset_impl();
    }
    rwlock_guard lock(&m_rwlock, true, __FILE__, __LINE__);
    return reset_impl();
  }

  /**
    Reset the buffer. Assumes an active read_lock() if the buffer is shared.
  */
  void reset_no_lock() { return reset_impl(); }

  void reset_impl() {
    m_index = 0;
    m_count = 0;
    m_full = false;
    m_wrapped = false;
  }

  /**
    @brief Map a global logical index to the current ring-buffer row.

    This helper translates a monotonically increasing logical sample index
    (e.g. a global “next_metric”) into a window-relative row index [0..count-1]
    for the current contents of the buffer.

    A logical index is considered “available” if it refers to an element that is
    still retained in the buffer (not yet overwritten.) If the logical index is
    older than the oldest existing element or newer than the most recent one,
    the mapping fails and the function returns false.

    Thread safety:
    - Acquires the buffer’s read lock internally (if the buffer is shared) to
      provide a coherent snapshot of count() and total_written().

    @param[in]  logical_idx  Global monotonically increasing sample index.
    @param[out] row_out      On success, the row index in the range [0, count())
                             that corresponds to logical_idx.

    @return true  If logical_idx maps to a currently available row.
    @return false If logical_idx is too old (overwritten) or not yet written.

    @note This function does not perform I/O. Use the returned @p row_out
          with read(record, row_out) to retrieve the element.
   */
  bool map_logical_to_row_no_lock(uint64_t logical_idx, int &row_out) const {
    return map_logical_to_row_impl(logical_idx, row_out);
  }

  bool map_logical_to_row(uint64_t logical_idx, int &row_out) const {
    rwlock_guard lock(&m_rwlock, false, __FILE__, __LINE__);
    return map_logical_to_row_impl(logical_idx, row_out);
  }

  bool map_logical_to_row_impl(uint64_t logical_idx, int &row_out) const {
    if (m_count == 0) {
      return false;
    }
    // The logical index must map to the existing rows [0..count-1]. If the
    // desired row was overwritten, or if the row does not yet exist (> count),
    // then the logical index is invalid.
    const uint64_t oldest_logical =
        (m_total_written >= m_count) ? (m_total_written - m_count) : 0;
    if (logical_idx < oldest_logical || logical_idx >= m_total_written) {
      return false;
    }
    row_out = static_cast<int>(logical_idx - oldest_logical);  // 0..count-1
    return (row_out >= 0 && std::cmp_less(row_out, m_count));
  }

  mysql_rwlock_t *get_rwlock() {
    // assert(m_initialized); // or some other check
    return &m_rwlock;
  }

  size_t max_size() const { return m_max_size; }
  size_t index() const { return m_index; }
  size_t count() const { return m_count; }
  [[nodiscard]] size_t total_written() const { return m_total_written; }
  bool full() const { return m_full; }
  bool wrapped() const { return m_wrapped; }

 protected:
  std::unique_ptr<std::vector<T, Allocator>> m_stats{nullptr};
  size_t m_max_size{0};
  size_t m_index{0};          // next element to be written
  size_t m_count{0};          // non-empty rows
  size_t m_total_written{0};  // monotonically increasing write count
  bool m_full{false};
  bool m_wrapped{false};
  bool m_no_lock{false};  // for non-shared buffers
  PSI_rwlock_key m_rwlock_key{0};
  mutable mysql_rwlock_t m_rwlock;  // some const methods use rwlock // NOLINT
  PSI_memory_key m_memory_key{0};
};

}  // namespace hm
