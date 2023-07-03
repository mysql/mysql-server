/*****************************************************************************

Copyright (c) 2017, 2022, Oracle and/or its affiliates.

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

Portions of this file contain modifications contributed and copyrighted by
Google, Inc. Those modifications are gratefully acknowledged and are described
briefly in the InnoDB documentation. The contributions by Google are
incorporated with their permission, and subject to the conditions contained in
the file COPYING.Google.

*****************************************************************************/

/**************************************************/ /**
 @file include/ut0link_buf.h

 Link buffer - concurrent data structure which allows:
         - concurrent addition of links
         - single-threaded tracking of connected path created by links
         - limited size of window with holes (missing links)

 Created 2017-08-30 Paweł Olchawa
 *******************************************************/

#ifndef ut0link_buf_h
#define ut0link_buf_h

#include <atomic>
#include <cstdint>

#include "ut0counter.h"
#include "ut0dbg.h"
#include "ut0new.h"
#include "ut0ut.h"

/** Concurrent data structure, which allows to track concurrently
performed operations which locally might be dis-ordered.

This data structure is informed about finished concurrent operations
and tracks up to which point in a total order all operations have
been finished (there are no holes).

It also allows to limit the last period in which there might be holes.
These holes refer to unfinished concurrent operations, which precede
in the total order some operations that are already finished.

Threads might concurrently report finished operations (lock-free).

Threads might ask for maximum currently known position in total order,
up to which all operations are already finished (lock-free).

Single thread might track the reported finished operations and update
maximum position in total order, up to which all operations are done.

You might look at current usages of this data structure in log0buf.cc.
*/
template <typename Position = uint64_t>
class Link_buf {
 public:
  /** Type used to express distance between two positions.
  It could become a parameter of template if it was useful.
  However there is no such need currently. */
  typedef Position Distance;

  /** Constructs the link buffer. Allocated memory for the links.
  Initializes the tail pointer with 0.

  @param[in]    capacity        number of slots in the ring buffer */
  explicit Link_buf(size_t capacity);

  Link_buf();

  Link_buf(Link_buf &&rhs);

  Link_buf(const Link_buf &rhs) = delete;

  Link_buf &operator=(Link_buf &&rhs);

  Link_buf &operator=(const Link_buf &rhs) = delete;

  /** Destructs the link buffer. Deallocates memory for the links. */
  ~Link_buf();

  /** Add a directed link between two given positions. It is user's
  responsibility to ensure that there is space for the link. This is
  because it can be useful to ensure much earlier that there is space.

  @param[in]    from    position where the link starts
  @param[in]    to      position where the link ends (from -> to) */
  void add_link(Position from, Position to);

  /** Add a directed link between two given positions. It is user's
  responsibility to ensure that there is space for the link. This is
  because it can be useful to ensure much earlier that there is space.
  In addition, advances the tail pointer in the buffer if possible.

  @param[in]    from    position where the link starts
  @param[in]    to      position where the link ends (from -> to) */
  void add_link_advance_tail(Position from, Position to);

  /** Advances the tail pointer in the buffer by following connected
  path created by links. Starts at current position of the pointer.
  Stops when the provided function returns true.

  @param[in]    stop_condition  function used as a stop condition;
                                  (lsn_t prev, lsn_t next) -> bool;
                                  returns false if we should follow
                                  the link prev->next, true to stop
  @param[in]    max_retry       max fails to retry

  @return true if and only if the pointer has been advanced */
  template <typename Stop_condition>
  bool advance_tail_until(Stop_condition stop_condition,
                          uint32_t max_retry = 1);

  /** Advances the tail pointer in the buffer without additional
  condition for stop. Stops at missing outgoing link.

  @see advance_tail_until()

  @return true if and only if the pointer has been advanced */
  bool advance_tail();

  /** @return capacity of the ring buffer */
  size_t capacity() const;

  /** @return the tail pointer */
  Position tail() const;

  /** Checks if there is space to add link at given position.
  User has to use this function before adding the link, and
  should wait until the free space exists.

  @param[in]    position        position to check

  @return true if and only if the space is free */
  bool has_space(Position position);

  /** Validates (using assertions) that there are no links set
  in the range [begin, end). */
  void validate_no_links(Position begin, Position end);

  /** Validates (using assertions) that there no links at all. */
  void validate_no_links();

 private:
  /** Translates position expressed in original unit to position
  in the m_links (which is a ring buffer).

  @param[in]    position        position in original unit

  @return position in the m_links */
  size_t slot_index(Position position) const;

  /** Computes next position by looking into slots array and
  following single link which starts in provided position.

  @param[in]    position        position to start
  @param[out]   next            computed next position

  @return false if there was no link, true otherwise */
  bool next_position(Position position, Position &next);

  /** Deallocated memory, if it was allocated. */
  void free();

  /** Capacity of the buffer. */
  size_t m_capacity;

  /** Pointer to the ring buffer (unaligned). */
  std::atomic<Distance> *m_links;

  /** Tail pointer in the buffer (expressed in original unit). */
  alignas(ut::INNODB_CACHE_LINE_SIZE) std::atomic<Position> m_tail;
};

template <typename Position>
Link_buf<Position>::Link_buf(size_t capacity)
    : m_capacity(capacity), m_tail(0) {
  if (capacity == 0) {
    m_links = nullptr;
    return;
  }

  ut_a((capacity & (capacity - 1)) == 0);

  m_links = ut::new_arr_withkey<std::atomic<Distance>>(UT_NEW_THIS_FILE_PSI_KEY,
                                                       ut::Count{capacity});

  for (size_t i = 0; i < capacity; ++i) {
    m_links[i].store(0);
  }
}

template <typename Position>
Link_buf<Position>::Link_buf() : Link_buf(0) {}

template <typename Position>
Link_buf<Position>::Link_buf(Link_buf &&rhs)
    : m_capacity(rhs.m_capacity), m_tail(rhs.m_tail.load()) {
  m_links = rhs.m_links;
  rhs.m_links = nullptr;
}

template <typename Position>
Link_buf<Position> &Link_buf<Position>::operator=(Link_buf &&rhs) {
  free();

  m_capacity = rhs.m_capacity;

  m_tail.store(rhs.m_tail.load());

  m_links = rhs.m_links;
  rhs.m_links = nullptr;

  return *this;
}

template <typename Position>
Link_buf<Position>::~Link_buf() {
  free();
}

template <typename Position>
void Link_buf<Position>::free() {
  if (m_links != nullptr) {
    ut::delete_arr(m_links);
    m_links = nullptr;
  }
}

template <typename Position>
inline void Link_buf<Position>::add_link(Position from, Position to) {
  ut_ad(to > from);
  ut_ad(to - from <= std::numeric_limits<Distance>::max());

  const auto index = slot_index(from);

  auto &slot = m_links[index];

  slot.store(to);
}

template <typename Position>
inline bool Link_buf<Position>::next_position(Position position,
                                              Position &next) {
  const auto index = slot_index(position);

  auto &slot = m_links[index];

  next = slot.load(std::memory_order_relaxed);

  return next <= position;
}

template <typename Position>
inline void Link_buf<Position>::add_link_advance_tail(Position from,
                                                      Position to) {
  ut_ad(to > from);
  ut_ad(to - from <= std::numeric_limits<Distance>::max());

  auto position = m_tail.load(std::memory_order_acquire);

  ut_ad(position <= from);

  if (position == from) {
    /* can advance m_tail directly and exclusively, and it is unlock */
    m_tail.store(to, std::memory_order_release);
  } else {
    auto index = slot_index(from);
    auto &slot = m_links[index];

    /* add link */
    slot.store(to, std::memory_order_release);

    auto stop_condition = [&](Position prev_pos, Position) {
      return (prev_pos > from);
    };

    advance_tail_until(stop_condition);
  }
}

template <typename Position>
template <typename Stop_condition>
bool Link_buf<Position>::advance_tail_until(Stop_condition stop_condition,
                                            uint32_t max_retry) {
  /* multi threaded aware */
  auto position = m_tail.load(std::memory_order_acquire);
  auto from = position;

  uint32_t retry = 0;
  while (true) {
    auto index = slot_index(position);
    auto &slot = m_links[index];

    auto next_load = slot.load(std::memory_order_acquire);

    if (next_load >= position + m_capacity) {
      /* either we wrapped and tail was advanced meanwhile,
      or there is link start_lsn -> end_lsn of length >= m_capacity */
      position = m_tail.load(std::memory_order_acquire);
      if (position != from) {
        from = position;
        continue;
      }
    }

    if (next_load <= position || stop_condition(position, next_load)) {
      /* nothing to advance for now */
      return false;
    }

    /* try to lock as storing the end */
    if (slot.compare_exchange_strong(next_load, position,
                                     std::memory_order_acq_rel)) {
      /* it could happen, that after thread read position = m_tail.load(),
      it got scheduled out for longer; when it comes back it might still
      see the link going forward in that slot but m_tail could have been
      already advanced forward (as we do not reset slots when traversing
      them); thread needs to re-check if m_tail is still behind the slot. */
      position = m_tail.load(std::memory_order_acquire);
      if (position == from) {
        /* confirmed. can advance m_tail exclusively */
        position = next_load;
        break;
      }
    }

    retry++;
    if (retry > max_retry) {
      /* give up */
      return false;
    }

    UT_RELAX_CPU();
    position = m_tail.load(std::memory_order_acquire);
    if (position == from) {
      /* no progress? */
      return false;
    }
    from = position;
  }

  while (true) {
    Position next;

    bool stop = next_position(position, next);

    if (stop || stop_condition(position, next)) {
      break;
    }

    position = next;
  }

  ut_a(from == m_tail.load(std::memory_order_acquire));

  /* unlock */
  m_tail.store(position, std::memory_order_release);

  if (position == from) {
    return false;
  }

  return true;
}

template <typename Position>
inline bool Link_buf<Position>::advance_tail() {
  auto stop_condition = [](Position, Position) { return false; };

  return advance_tail_until(stop_condition);
}

template <typename Position>
inline size_t Link_buf<Position>::capacity() const {
  return m_capacity;
}

template <typename Position>
inline Position Link_buf<Position>::tail() const {
  return m_tail.load(std::memory_order_acquire);
}

template <typename Position>
inline bool Link_buf<Position>::has_space(Position position) {
  auto tail = m_tail.load(std::memory_order_acquire);
  if (tail + m_capacity > position) {
    return true;
  }

  auto stop_condition = [](Position, Position) { return false; };
  advance_tail_until(stop_condition, 0);

  tail = m_tail.load(std::memory_order_acquire);
  return tail + m_capacity > position;
}

template <typename Position>
inline size_t Link_buf<Position>::slot_index(Position position) const {
  return position & (m_capacity - 1);
}

template <typename Position>
void Link_buf<Position>::validate_no_links(Position begin, Position end) {
  const auto tail = m_tail.load();

  /* After m_capacity iterations we would have all slots tested. */

  end = std::min(end, begin + m_capacity);

  for (; begin < end; ++begin) {
    const size_t index = slot_index(begin);

    const auto &slot = m_links[index];

    ut_a(slot.load() <= tail);
  }
}

template <typename Position>
void Link_buf<Position>::validate_no_links() {
  validate_no_links(0, m_capacity);
}

#endif /* ut0link_buf_h */
