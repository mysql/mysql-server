/*****************************************************************************
Copyright (c) 2017, 2023, Oracle and/or its affiliates.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License, version 2.0,
as published by the Free Software Foundation.

This program is also distributed with certain software (including
but not limited to OpenSSL) that is licensed under separate terms,
as designated in a particular file or component or in included license
documentation.  The authors of MySQL hereby grant you an additional
permission to link the program and your derivative works with the
separately licensed software that they have included with MySQL.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License, version 2.0, for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

*****************************************************************************/

#ifndef ut0mpmcbq_h
#define ut0mpmcbq_h

#include "ut0cpu_cache.h"

#include <atomic>

/** Multiple producer consumer, bounded queue
 Implementation of Dmitry Vyukov's MPMC algorithm
 http://www.1024cores.net/home/lock-free-algorithms/queues/bounded-mpmc-queue */
template <typename T>
class mpmc_bq {
 public:
  /** Constructor
  @param[in]    n_elems         Max number of elements allowed */
  explicit mpmc_bq(size_t n_elems)
      : m_ring(reinterpret_cast<Cell *>(ut::new_arr_withkey<Aligned>(
            UT_NEW_THIS_FILE_PSI_KEY, ut::Count{n_elems}))),
        m_capacity(n_elems - 1) {
    /* Should be a power of 2 */
    ut_a((n_elems >= 2) && ((n_elems & (n_elems - 1)) == 0));

    for (size_t i = 0; i < n_elems; ++i) {
      m_ring[i].m_pos.store(i, std::memory_order_relaxed);
    }

    m_enqueue_pos.store(0, std::memory_order_relaxed);
    m_dequeue_pos.store(0, std::memory_order_relaxed);
  }

  /** Destructor */
  ~mpmc_bq() { ut::delete_arr(m_ring); }

  /** Enqueue an element
  @param[in]    data            Element to insert, it will be copied
  @return true on success */
  [[nodiscard]] bool enqueue(T const &data) {
    /* m_enqueue_pos only wraps at MAX(m_enqueue_pos), instead
    we use the capacity to convert the sequence to an array
    index. This is why the ring buffer must be a size which
    is a power of 2. This also allows the sequence to double
    as a ticket/lock. */

    size_t pos = m_enqueue_pos.load(std::memory_order_relaxed);

    Cell *cell;

    for (;;) {
      cell = &m_ring[pos & m_capacity];

      size_t seq;

      seq = cell->m_pos.load(std::memory_order_acquire);

      intptr_t diff = (intptr_t)seq - (intptr_t)pos;

      /* If they are the same then it means this cell is empty */

      if (diff == 0) {
        /* Claim our spot by moving head. If head isn't the same as we last
        checked then that means someone beat us to the punch. Weak compare is
        faster, but can return spurious results which in this instance is OK,
        because it's in the loop */

        if (m_enqueue_pos.compare_exchange_weak(pos, pos + 1,
                                                std::memory_order_relaxed)) {
          break;
        }

      } else if (diff < 0) {
        /* The queue is full */

        return (false);

      } else {
        pos = m_enqueue_pos.load(std::memory_order_relaxed);
      }
    }

    cell->m_data = data;

    /* Increment the sequence so that the tail knows it's accessible */

    cell->m_pos.store(pos + 1, std::memory_order_release);

    return (true);
  }

  /** Dequeue an element
  @param[out]   data            Element read from the queue
  @return true on success */
  [[nodiscard]] bool dequeue(T &data) {
    Cell *cell;
    size_t pos = m_dequeue_pos.load(std::memory_order_relaxed);

    for (;;) {
      cell = &m_ring[pos & m_capacity];

      size_t seq = cell->m_pos.load(std::memory_order_acquire);

      auto diff = (intptr_t)seq - (intptr_t)(pos + 1);

      if (diff == 0) {
        /* Claim our spot by moving the head. If head isn't the same as we last
        checked then that means someone beat us to the punch. Weak compare is
        faster, but can return spurious results. Which in this instance is
        OK, because it's in the loop. */

        if (m_dequeue_pos.compare_exchange_weak(pos, pos + 1,
                                                std::memory_order_relaxed)) {
          break;
        }

      } else if (diff < 0) {
        /* The queue is empty */
        return (false);

      } else {
        /* Under normal circumstances this branch should never be taken. */
        pos = m_dequeue_pos.load(std::memory_order_relaxed);
      }
    }

    data = cell->m_data;

    /* Set the sequence to what the head sequence should be next
    time around */

    cell->m_pos.store(pos + m_capacity + 1, std::memory_order_release);

    return (true);
  }

  /** @return the capacity of the queue */
  [[nodiscard]] size_t capacity() const { return (m_capacity + 1); }

  /** @return true if the queue is empty. */
  [[nodiscard]] bool empty() const {
    size_t pos = m_dequeue_pos.load(std::memory_order_relaxed);

    for (;;) {
      auto cell = &m_ring[pos & m_capacity];

      size_t seq = cell->m_pos.load(std::memory_order_acquire);

      auto diff = (intptr_t)seq - (intptr_t)(pos + 1);

      if (diff == 0) {
        return (false);
      } else if (diff < 0) {
        return (true);
      } else {
        pos = m_dequeue_pos.load(std::memory_order_relaxed);
      }
    }
  }

 private:
  using Pad = byte[ut::INNODB_CACHE_LINE_SIZE];

  struct Cell {
    std::atomic<size_t> m_pos;
    T m_data;
  };

  using Aligned =
      typename std::aligned_storage<sizeof(Cell),
                                    std::alignment_of<Cell>::value>::type;

  Pad m_pad0;
  Cell *const m_ring;
  size_t const m_capacity;
  Pad m_pad1;
  std::atomic<size_t> m_enqueue_pos;
  Pad m_pad2;
  std::atomic<size_t> m_dequeue_pos;
  Pad m_pad3;

  mpmc_bq(mpmc_bq &&) = delete;
  mpmc_bq(const mpmc_bq &) = delete;
  mpmc_bq &operator=(mpmc_bq &&) = delete;
  mpmc_bq &operator=(const mpmc_bq &) = delete;
};

#endif /* ut0mpmcbq_h */
