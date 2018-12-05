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

#ifndef MYSQL_HARNESS_MPSC_UNBOUND_QUEUE_INCLUDED
#define MYSQL_HARNESS_MPSC_UNBOUND_QUEUE_INCLUDED

#include <atomic>

#include "mysql/harness/waiting_queue_adaptor.h"

namespace mysql_harness {

/**
 * a unbounded multi-producer single-consumer queue.
 *
 * multiple threads can write at the same time into the queue, only one can read
 *
 * http://www.1024cores.net/home/lock-free-algorithms/queues/non-intrusive-mpsc-node-based-queue
 */
template <typename T>
class MPSCQueueDV {
 public:
  using value_type = T;

  MPSCQueueDV()
      : head_{new Node}, tail_{head_.load(std::memory_order_relaxed)} {}

  ~MPSCQueueDV() {
    T item;

    // free all items in the queue
    while (dequeue(item)) {
    }

    // free the head node
    Node *front = head_.load(std::memory_order_relaxed);
    delete front;
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
    node->next.store(nullptr, std::memory_order_relaxed);

    Node *prev_head = head_.exchange(node, std::memory_order_acq_rel);
    prev_head->next.store(node, std::memory_order_release);

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
    Node *tail = tail_.load(std::memory_order_relaxed);
    Node *next = tail->next.load(std::memory_order_acquire);

    if (next == nullptr) {
      return false;
    }

    item = std::move(next->data);

    tail_.store(next, std::memory_order_relaxed);

    delete tail;

    return true;
  }

 private:
  MPSCQueueDV(const MPSCQueueDV &) = delete;
  void operator=(const MPSCQueueDV &) = delete;

  struct Node {
    T data;
    std::atomic<Node *> next{nullptr};
  };

  std::atomic<Node *> head_;
  std::atomic<Node *> tail_;
};

// allow to switch implementation later
template <typename T>
using MPSCQueue = MPSCQueueDV<T>;

template <typename T>
using WaitingMPSCQueue = WaitingQueueAdaptor<MPSCQueue<T>>;

}  // namespace mysql_harness

#endif
