// Copyright (c) 2026, Oracle and/or its affiliates.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License, version 2.0,
// as published by the Free Software Foundation.
//
// This program is designed to work with certain software (including
// but not limited to OpenSSL) that is licensed under separate terms,
// as designated in a particular file or component or in included license
// documentation.  The authors of MySQL hereby grant you an additional
// permission to link the program and your derivative works with the
// separately licensed software that they have either included with
// the program or referenced in the documentation.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License, version 2.0, for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA.

#include "mysql/concurrency/sync_bounded_queue.h"

namespace mysql::concurrency {

template <typename T, uint64_t capacity_tp>
  requires Is_power_of_two<capacity_tp>
Sync_bounded_queue<T, capacity_tp>::Sync_bounded_queue(Memory_resource) {}

template <typename T, uint64_t capacity_tp>
  requires Is_power_of_two<capacity_tp>
template <typename P>
bool Sync_bounded_queue<T, capacity_tp>::enqueue(Element_type &&element,
                                                 P &&stop_predicate) {
  auto idx = get_next_index(m_head);
  auto &slot = m_slots[idx];
  // In case we don't have space to save an item (unlikely to happen),
  // we wait on condition variable
  // @todo consider setting a stage
  // @todo avoid blocking producer and return a boolean - implement try_enqueue
  {
    std::unique_lock<Mutex_type> lock(slot.validity_mt);
    slot.producer_cv.wait(lock,
                          [&]() { return !slot.validity || stop_predicate(); });
    if (slot.validity) {
      return false;
    }
    // now we can insert
    slot.element = std::move(element);
    slot.validity = true;
  }
  // Notify consumers waiting for full slot
  slot.consumer_cv.notify_one();
  return true;
}

template <typename T, uint64_t capacity_tp>
  requires Is_power_of_two<capacity_tp> bool
Sync_bounded_queue<T, capacity_tp>::enqueue(Element_type &&element) {
  return enqueue(std::move(element), []() { return false; });
}

template <typename T, uint64_t capacity_tp>
  requires Is_power_of_two<capacity_tp>
template <typename P>
std::pair<T, bool> Sync_bounded_queue<T, capacity_tp>::dequeue(
    P &&stop_predicate) {
  auto idx = get_next_index(m_tail);
  // we may block in case there are no elements, likely to happen when
  // using a large amount of consumers
  auto &slot = m_slots[idx];
  std::unique_lock<Mutex_type> lock(slot.validity_mt);
  // Wait for element or stop (exits immediately if either true)
  slot.consumer_cv.wait(lock, [&stop_predicate, &slot]() {
    return slot.validity || stop_predicate();
  });
  if (slot.validity) {
    // Consume the element
    T elem = std::move(slot.element);
    slot.validity = false;
    lock.unlock();
    // Notify producers waiting for empty slot
    slot.producer_cv.notify_one();
    return {std::move(elem), true};
  } else {
    // Stop signaled, no element (predicate must be true since wait exited)
    return {T{}, false};
  }
}

template <typename T, uint64_t capacity_tp>
  requires Is_power_of_two<capacity_tp> bool
Sync_bounded_queue<T, capacity_tp>::empty() {
  // Exact emptiness check, handling wrap-around.
  // Define M = std::numeric_limits<size_t>() + 1 - wrap point.
  // Assumes max threads P < M/2 and ops N < M/2 between loads, where M is the
  // wrap point. Returns true if no elements (head "ahead" of tail by less than
  // half capacity).
  auto tail_idx = m_tail.load();
  auto head_idx = m_head.load();
  return (tail_idx - head_idx) < (std::numeric_limits<size_t>::max() / 2);
}

template <typename T, uint64_t capacity_tp>
  requires Is_power_of_two<capacity_tp>
std::size_t Sync_bounded_queue<T, capacity_tp>::get_next_index(
    std::atomic<std::size_t> &current) {
  return current.fetch_add(1, std::memory_order_relaxed) % m_capacity;
}

template <typename T, uint64_t capacity_tp>
  requires Is_power_of_two<capacity_tp>
void Sync_bounded_queue<T, capacity_tp>::notify_all() {
  // Wake all waiting threads to check stop_predicate
  for (std::size_t idx = 0; idx < m_capacity; ++idx) {
    m_slots[idx].producer_cv.notify_all();
    m_slots[idx].consumer_cv.notify_all();
  }
}

template <typename T, uint64_t capacity_tp>
  requires Is_power_of_two<capacity_tp>
std::size_t Sync_bounded_queue<T, capacity_tp>::size() const {
  auto head = m_head.load();
  auto tail = m_tail.load();
  if (head < tail) {
    if (tail - head >= std::numeric_limits<std::size_t>::max() / 2) {
      // head wrapped
      return head - tail;
    } else {
      // there are waiting consumers, and/or tail raced ahead of head between
      // the two atomic loads
      return 0;
    }
  } else {
    // Normal case
    return head - tail;
  }
}

template <typename T, uint64_t capacity_tp>
  requires Is_power_of_two<capacity_tp>
void Sync_bounded_queue<T, capacity_tp>::reset() {
  for (auto &slot : m_slots) {
    std::unique_lock<Mutex_type> lock(slot.validity_mt);
    slot.validity = false;
    slot.element = T{};
  }
  m_head.store(0);
  m_tail.store(0);
}

}  // namespace mysql::concurrency
