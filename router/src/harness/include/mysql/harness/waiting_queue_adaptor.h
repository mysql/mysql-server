/*
  Copyright (c) 2018, 2022, Oracle and/or its affiliates.

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
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef MYSQL_HARNESS_WAITING_QUEUE_ADAPTOR_INCLUDED
#define MYSQL_HARNESS_WAITING_QUEUE_ADAPTOR_INCLUDED

#include <condition_variable>
#include <mutex>

namespace mysql_harness {

/**
 * provide waiting pop and push operator to thread-safe queues.
 *
 */
template <class Q>
class WaitingQueueAdaptor {
 public:
  using value_type = typename Q::value_type;

  /**
   * dequeue an item from a queue.
   *
   * Waits until item becomes available.
   *
   * @returns item
   */
  value_type pop() {
    value_type item;
    {
      std::unique_lock<std::mutex> lk(dequeueable_cond_mutex_);

      dequeueable_cond_.wait(lk, [this, &item] { return q_.dequeue(item); });
    }

    notify_enqueueable();

    return item;
  }

  /**
   * dequeue an item from a queue if queue is not empty.
   *
   * @param item dequeued item if queue was not empty
   *
   * @returns item
   * @retval true item dequeued
   * @retval false queue was empty
   */
  bool try_pop(value_type &item) {
    if (false == q_.dequeue(item)) {
      return false;
    }
    notify_enqueueable();

    return true;
  }

  /**
   * enqueue item into queue.
   *
   * waits until queue is not full anymore.
   *
   * @param item item to enqueue
   */
  void push(const value_type &item) {
    {
      std::unique_lock<std::mutex> lk(enqueueable_cond_mutex_);

      enqueueable_cond_.wait(lk, [this, &item] { return q_.enqueue(item); });
    }
    notify_dequeueable();
  }

  void push(value_type &&item) {
    {
      std::unique_lock<std::mutex> lk(enqueueable_cond_mutex_);

      enqueueable_cond_.wait(
          lk, [this, &item] { return q_.enqueue(std::move(item)); });
    }
    notify_dequeueable();
  }

  /**
   * enqueue an item into a queue if queue is not full.
   *
   * @param item item to enqueue
   *
   * @returns item
   * @retval true item enqueued
   * @retval false queue was full
   */
  bool try_push(const value_type &item) {
    if (false == q_.enqueue(item)) {
      return false;
    }

    notify_dequeueable();
  }

 private:
  void notify_dequeueable() {
    std::unique_lock<std::mutex> lk(dequeueable_cond_mutex_);

    dequeueable_cond_.notify_all();
  }
  void notify_enqueueable() {
    std::unique_lock<std::mutex> lk(enqueueable_cond_mutex_);

    enqueueable_cond_.notify_all();
  }
  Q q_;

  std::mutex dequeueable_cond_mutex_;
  std::condition_variable dequeueable_cond_;

  std::mutex enqueueable_cond_mutex_;
  std::condition_variable enqueueable_cond_;
};

}  // namespace mysql_harness

#endif
