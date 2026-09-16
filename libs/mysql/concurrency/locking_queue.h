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

#ifndef MYSQL_CONCURRENCY_LOCKING_QUEUE
#define MYSQL_CONCURRENCY_LOCKING_QUEUE

#include <condition_variable>
#include <mutex>
#include <queue>

#include "mysql/allocators/memory_resource.h"
#include "mysql/concurrency/condition_variable.h"
#include "mysql/concurrency/mutex.h"

/// @addtogroup GroupLibsMysqlConcurrency
/// @{

namespace mysql::concurrency {

/// @brief Synchronized unbounded FIFO queue for thread-safe producer-consumer
///        operations, using a single mutex and condition variable.
/// @details
/// This queue is unbounded (enqueue always succeeds). For performance, it does
/// not enforce safe destruction during concurrent access. Ensure the queue
/// outlives all producers and consumers to avoid undefined behavior.
template <typename ET>
class Locking_queue {
 public:
  using ElemType = ET;
  using Memory_resource = mysql::allocators::Memory_resource;

  /// @brief Constructs the queue. The memory_resource is reserved for future
  ///        use (e.g., custom allocation for elements) and currently unused.
  Locking_queue(Memory_resource memory_resource = {})
      : m_mutex_access_queue(MYSQL_CONCURRENCY_DEFINE_MT_PSI_KEY(0)),
        m_cv_empty_queue(MYSQL_CONCURRENCY_DEFINE_CV_PSI_KEY(0)),
        m_memory_resource(memory_resource) {}
  // Disable copy-move semantics
  Locking_queue(const Locking_queue<ElemType> &) = delete;
  Locking_queue(Locking_queue<ElemType> &&) = delete;
  Locking_queue &operator=(const Locking_queue<ElemType> &src) = delete;
  Locking_queue &operator=(Locking_queue<ElemType> &&src) = delete;
  ~Locking_queue() = default;

  /// @brief Enqueues an element into the queue (called by producers).
  /// @param elem Element to enqueue (moved).
  /// @return Always true, as the queue is unbounded and enqueue always
  /// succeeds.
  /// @details Acquires the mutex, pushes the element to the back, notifies one
  ///          waiting consumer, then releases the mutex.
  bool enqueue(ElemType &&elem) {
    std::lock_guard<concurrency::Mutex> lock(m_mutex_access_queue);
    m_queue.push(std::move(elem));
    m_cv_empty_queue.notify_one();
    return true;
  }

  /// @brief Checks if the queue is empty.
  /// @details Acquires the mutex for thread-safe access. Due to concurrent
  ///          enqueues/consumes, the result is a snapshot and may change
  ///          immediately after the call.
  /// @return true if no elements are present at the time of the check.
  bool empty() const {
    std::lock_guard<concurrency::Mutex> lock(m_mutex_access_queue);
    return m_queue.empty();
  }

  /// @brief Consumes (dequeues) an element from the front of the queue.
  ///        Blocks until an element is available or the stop predicate
  ///        triggers.
  /// @param pred Stop predicate (callable); if pred() returns true after
  /// waking, stops waiting and returns without consuming.
  /// @return std::pair<ElemType, bool>: {consumed element, true} if an element
  /// was dequeued, or {default-constructed ElemType, false} if stopped by
  /// pred() without consuming.
  template <typename P>
  std::pair<ElemType, bool> dequeue(P &&pred) {
    std::unique_lock<concurrency::Mutex> lock(m_mutex_access_queue);
    auto stop_waiting = [this, &pred]() -> bool {
      return pred() || !m_queue.empty();
    };
    m_cv_empty_queue.wait(lock, stop_waiting);
    if (m_queue.empty()) {
      assert(stop_waiting());
      return std::make_pair(ElemType{}, false);
    }
    auto elem = m_queue.front();
    m_queue.pop();
    return std::make_pair(elem, true);
  }

  /// @brief Wakes all waiting consumers (e.g., during shutdown to check pred
  /// and exit). Notifies all on the condition variable, unblocking blocked
  /// consumes.
  void notify_all() { m_cv_empty_queue.notify_all(); }

 private:
  /// Underlying unbounded FIFO queue.
  std::queue<ElemType> m_queue;
  /// Mutex protecting all queue operations.
  mutable concurrency::Mutex m_mutex_access_queue;
  /// Condition variable for waiting on non-empty queue.
  concurrency::Condition_variable m_cv_empty_queue;
  /// Reserved for future custom allocation; currently unused.
  Memory_resource m_memory_resource;
};

}  // namespace mysql::concurrency

/// @}

#endif  // MYSQL_CONCURRENCY_LOCKING_QUEUE
