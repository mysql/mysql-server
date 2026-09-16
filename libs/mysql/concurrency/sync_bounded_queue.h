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

#ifndef MYSQL_CONCURRENCY_SYNC_BOUNDED_QUEUE
#define MYSQL_CONCURRENCY_SYNC_BOUNDED_QUEUE

#include <array>
#include <bit>
#include <condition_variable>
#include <cstdint>
#include <limits>
#include <ostream>

#include "mysql/allocators/allocator.h"
#include "mysql/concurrency/condition_variable.h"
#include "mysql/concurrency/mutex.h"

#include "mysql/concurrency/padded_slot.h"
#include "sql/containers/integrals_lockfree_queue.h"

namespace mysql::concurrency {

template <uint64_t N>
concept Is_power_of_two = std::has_single_bit(N);

/// @brief Bounded concurrent queue supporting multiple producers and consumers.
///        Designed to minimize contention using lock-free index allocation
///        followed by per-slot locking for safe element access and
///        notification.
///
/// ---Implementation:
/// The queue uses a fixed-size circular buffer implemented as a std::array
/// of Padded_slot structures. Each slot contains:
/// - The element (T)
/// - A bool validity flag indicating if the slot holds a valid element
/// - A mutex (validity_mt) protecting the validity and element
/// - Two condition variables: producer_cv (wait for empty slot) and
///   consumer_cv (wait for valid element)
///
/// Producers:
/// - Lock-free: Atomically fetch_add(1) on m_head to get a unique index
///   (m_head is the next producer position).
/// - Lock slot mutex, wait on producer_cv until !validity (empty slot).
/// - Assign element to slot, set validity = true.
/// - Unlock and notify one waiting consumer on consumer_cv.
///
/// Consumers:
/// - Lock-free: Atomically fetch_add(1) on m_tail to get a unique index
///   (m_tail is the next consumer position).
/// - Lock slot mutex, wait on consumer_cv until validity (valid element).
/// - If stop_predicate() is true, return empty pair with false.
/// - Otherwise, move element, set validity = false.
/// - Unlock and notify one waiting producer on producer_cv.
///
/// Indices wrap around using % m_capacity (capacity is power of 2).
/// The queue has fixed capacity; enqueue blocks if full (no resizing).
/// Contention is low if capacity >= num_producers + num_consumers.
/// Index allocation is lock-free, but slot access uses per-slot mutex/CV.
///
/// @tparam T Type of element stored in the queue.
/// @tparam capacity_tp Fixed capacity of the queue (must be power of 2).
template <typename T, uint64_t capacity_tp>
  requires Is_power_of_two<capacity_tp>
class Sync_bounded_queue {
  static_assert(std::has_single_bit(capacity_tp),
                "Capacity must be a power of 2");

 public:
  using Element_type = T;
  using Allocator = mysql::allocators::Allocator<T>;
  using Memory_resource = mysql::allocators::Memory_resource;
  using Mutex_type = std::mutex;
  using Cv_type = std::condition_variable;
  static constexpr uint64_t m_capacity = capacity_tp;

  /// @brief Construct the queue. The memory_resource parameter is reserved
  ///        for future use and currently ignored.
  Sync_bounded_queue(Memory_resource = {});

  /// @brief Enqueue (push) an element into the queue. Blocks the calling
  ///        producer if the queue is full (no space available).
  /// @details
  /// - Lock-free: Advances m_head via atomic fetch_add to get unique index.
  /// - Locks the slot mutex and waits on producer_cv until the slot is empty
  ///   (!validity).
  /// - Assigns the element and sets validity = true.
  /// - Notifies one waiting consumer on consumer_cv.
  /// The operation is lock-free for index allocation but uses per-slot locking
  /// for safe access. Blocks only if full; choose capacity >= num_producers +
  /// num_consumers to minimize blocking.
  /// @param element Element to enqueue
  /// @param stop_predicate Predicate checked after wakeup; if true and
  ///                       still cannot enqueue, stops
  ///                       waiting and returns {default T, false}.
  template <typename P>
  bool enqueue(Element_type &&element, P &&stop_predicate);

  /// Overload without stop_predicate, using default that always returns false.
  /// @param element Element to enqueue
  bool enqueue(Element_type &&element);

  /// @brief Dequeue an element from the queue. Called by consumers.
  /// @param stop_predicate Predicate checked after wakeup; if true and
  ///                       still cannot dequeue, stops
  ///                       waiting and returns {default T, false}.
  /// @return Pair of {consumed element, true} if successful, or {default T,
  /// false}
  ///         if stopped by predicate.
  /// @details
  /// - Lock-free: Advances m_tail via atomic fetch_add to get unique index.
  /// - Locks the slot mutex and waits on consumer_cv until the slot is valid
  ///   (validity == true).
  /// - If stop_predicate() is true, returns without consuming.
  /// - Otherwise, moves the element and sets validity = false.
  /// - Notifies one waiting producer on producer_cv.
  /// The operation is lock-free for index allocation but uses per-slot locking
  /// for safe access. Blocks if empty until an element is available or stopped.
  template <typename P>
  [[nodiscard]] std::pair<Element_type, bool> dequeue(P &&stop_predicate);

  /// @brief Check if the queue is empty.
  /// @returns True when queue is empty, false otherwise.
  [[nodiscard]] bool empty();

  /// @brief Signal shutdown: Notifies all blocked consumers. Allows waiting
  /// consumers to wake and check stop_predicate to exit gracefully. Producer
  /// stop must be handled by the structure user, before calling "notify_all".
  void notify_all();

  /// Destructor
  /// Before deleting the queue, the caller must ensure that no thread
  /// is or will be using the queue, for instance, by first calling notify_all
  /// and then joining all consumer threads.
  virtual ~Sync_bounded_queue() = default;

  /// @brief Estimates the queue size based on m_head and m_tail positions.
  /// @details Due to concurrent modifications, this is an approximation.
  /// The actual size may be bigger than the returned value; the difference is
  /// at most min(N,M) where N and M are the number of concurrent
  /// enqueue/dequeue operations.
  /// @return Estimated number of elements (unconsumed valid slots).
  [[nodiscard]] std::size_t size() const;

  /// @brief Reset the queue: Clears all slots (sets validity=false, elements
  ///        to default), resets m_head and m_tail to 0.
  void reset();

 private:
  /// @brief Atomically increments the given position (m_head for producers,
  ///        m_tail for consumers) and returns the new index modulo capacity.
  ///        Used internally for lock-free index allocation.
  /// @param[in,out] current Atomic position to advance (m_head or m_tail).
  /// @return The granted index after increment (wrapped).
  [[nodiscard]] std::size_t get_next_index(std::atomic<std::size_t> &current);

  /// Padded slots containing elements and synchronization primitives
  std::array<detail::Padded_slot<Element_type, Mutex_type, Cv_type>,
             capacity_tp>
      m_slots;
  /// Current head idx, when at the end, wraps to 0
  std::atomic<std::size_t> m_head{0};
  /// Current tail idx, when at the end, wraps to 0
  std::atomic<std::size_t> m_tail{0};
};

}  // namespace mysql::concurrency

#include "mysql/concurrency/sync_bounded_queue_impl.hpp"

#endif  // MYSQL_CONCURRENCY_SYNC_BOUNDED_QUEUE
