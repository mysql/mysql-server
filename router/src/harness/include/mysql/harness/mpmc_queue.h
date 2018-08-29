/*
  Copyright (c) 2018, Oracle and/or its affiliates. All rights reserved.

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

#ifndef MYSQL_HARNESS_MPMC_UNBOUND_QUEUE_INCLUDED
#define MYSQL_HARNESS_MPMC_UNBOUND_QUEUE_INCLUDED

#include <mutex>

#include "mysql/harness/waiting_queue_adaptor.h"

namespace mysql_harness {

/**
 * a unbounded multi-producer multi-consumer queue.
 *
 * multiple threads can read and write at the same time into the queue
 *
 * - enqueue and dequeue do not block each other
 *
 * see:
 *   - Micheal & Scott: two-lock concurrent queue
 *   - "An Unbounded Total Queue" in "The Art of MultiProcessor Programming"
 */

/*
 * Implementation works everywhere, where mutex are available. If atomics and
 * CAS/LL_SC are available faster implementations could be used:
 *
 * - https://github.com/cameron314/concurrentqueue
 *
 * Micheal & Scott's "lock-free" algorithm mentioned in the 1996 paper requires
 * GC and hazard pointers, but here is also:
 *
 * - https://idea.popcount.org/2012-09-11-concurrent-queue-in-c/
 */
template <typename T>
class MPMCQueueMS2Lock {
 public:
  using value_type = T;

  MPMCQueueMS2Lock() : head_{new Node}, tail_{head_} {}

  ~MPMCQueueMS2Lock() {
    T item;

    // free all items in the queue
    while (dequeue(item)) {
    };

    // free the head node
    delete head_;
  }

  /**
   * enqueue an element.
   *
   * @param item item to enqueue
   *
   * @returns if item was enqueued
   * @retval true item got assigned to queue
   * @retval false queue is full
   */
  bool enqueue(const T &item) {
    Node *node = new Node;
    node->data = item;

    {
      std::unique_lock<std::mutex> lk(tail_mutex_);

      tail_->next = node;
      tail_ = node;
    }

    return true;
  }

  /**
   * try to dequeue element.
   *
   * @param item location of dequeued item if dequeue() was successful
   *
   * @returns if item was written
   * @retval true first item removed from the queue and assigned to item
   * @retval false queue is empty
   */
  bool dequeue(T &item) {
    Node *node = nullptr;
    {
      std::unique_lock<std::mutex> lk(head_mutex_);

      node = head_;
      Node *new_head = node->next;

      if (new_head == nullptr) {
        return false;
      }

      item = std::move(new_head->data);

      head_ = new_head;
    }

    delete node;

    return true;
  }

 private:
  struct Node {
    T data;
    Node *next{nullptr};
  };

  std::mutex head_mutex_;
  std::mutex tail_mutex_;
  Node *head_;
  Node *tail_;
};

// allow to switch implementation to a lock-free implementation later
template <typename T>
using MPMCQueue = MPMCQueueMS2Lock<T>;

template <typename T>
using WaitingMPMCQueue = WaitingQueueAdaptor<MPMCQueue<T>>;

}  // namespace mysql_harness

#endif
