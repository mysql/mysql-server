/*****************************************************************************
Copyright (c) 2017, 2018 Oracle and/or its affiliates. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA

*****************************************************************************/

#ifndef ut0mpmcbq_h
#define ut0mpmcbq_h

#include <atomic>

/** Multiple producer consumer, bounded queue
 Implementation of Dmitry Vyukov's MPMC algorithm
 http://www.1024cores.net/home/lock-free-algorithms/queues/bounded-mpmc-queue */
template <typename T>
class mpmc_bq {
 public:
  /** Constructor
  @param[in]	n_elems		Max number of elements allowed */
  explicit mpmc_bq(size_t n_elems)
      : m_ring(
            reinterpret_cast<cell_t *>(UT_NEW_ARRAY_NOKEY(aligned_t, n_elems))),
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
  ~mpmc_bq() { UT_DELETE_ARRAY(m_ring); }

  /** Enqueue an element
  @param[in]	data		Element to insert, it will be copied
  @return true on success */
  bool enqueue(T const &data) {
    /* m_enqueue_pos only wraps at MAX(m_enqueue_pos), instead we use the
    capacity to convert the sequence to an array index. This is why the ring
    buffer must be a size which is a power of 2. This also allows the
    sequence to double as a ticket/lock. */

    size_t pos = m_enqueue_pos.load(std::memory_order_relaxed);

    cell_t *cell;

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
  @param[out]	data		Element read from the queue
  @return true on success */
  bool dequeue(T &data) {
    cell_t *cell;
    size_t pos = m_dequeue_pos.load(std::memory_order_relaxed);

    for (;;) {
      cell = &m_ring[pos & m_capacity];

      size_t seq = cell->m_pos.load(std::memory_order_acquire);

      intptr_t diff;

      diff = (intptr_t)seq - (intptr_t)(pos + 1);

      /* If they are the same then it means this slot
      is empty */

      if (diff == 0) {
        /* Claim our spot by moving the head. If head isn't the same as we last
        checked then that means someone beat us to the punch. Weak compare is
        faster, but can return spurious results. Which in this instance is
        OK, because it's in the loop */

        if (m_dequeue_pos.compare_exchange_weak(pos, pos + 1,
                                                std::memory_order_relaxed)) {
          break;
        }

      } else if (diff < 0) {
        /* The queue is empty */

        return (false);

      } else {
        /* Under normal circumstances this branch
        should never be taken */

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
  size_t capacity() const { return (m_capacity + 1); }

 private:
  using pad_t = byte[INNOBASE_CACHE_LINE_SIZE];

  struct cell_t {
    std::atomic<size_t> m_pos;
    T m_data;
  };

  typedef typename std::aligned_storage<
      sizeof(cell_t), std::alignment_of<cell_t>::value>::type aligned_t;

  pad_t m_pad0;
  cell_t *const m_ring;
  size_t const m_capacity;
  pad_t m_pad1;
  std::atomic<size_t> m_enqueue_pos;
  pad_t m_pad2;
  std::atomic<size_t> m_dequeue_pos;
  pad_t m_pad3;

  // Disable copying
  mpmc_bq(mpmc_bq &&) = delete;
  mpmc_bq(mpmc_bq const &) = delete;
  mpmc_bq &operator=(mpmc_bq &&) = delete;
  mpmc_bq &operator=(mpmc_bq const &) = delete;
};

#endif /* ut0mpmcbq_h */
