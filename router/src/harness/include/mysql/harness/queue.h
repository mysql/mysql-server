/*
  Copyright (c) 2015, 2018, Oracle and/or its affiliates. All rights reserved.

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

#ifndef MYSQL_HARNESS_QUEUE_INCLUDED
#define MYSQL_HARNESS_QUEUE_INCLUDED

#include <atomic>
#include <condition_variable>
#include <iostream>
#include <memory>
#include <mutex>

namespace mysql_harness {

/**
 * Thread-safe queue.
 *
 * This class implements a thread-safe queue allowing multiple
 * simultaneous readers and writers.
 */
template <class T>
class queue {
  friend std::ostream &operator<<(std::ostream &out, const queue &q) {
    std::lock(q.head_mtx_, q.tail_mtx_);
    std::lock_guard<std::mutex> l1(q.head_mtx_, std::adopt_lock);
    std::lock_guard<std::mutex> l2(q.tail_mtx_, std::adopt_lock);
    for (Node *ptr = q.head_.get(); ptr != q.tail_; ptr = ptr->next_.get()) {
      out << *ptr->data_;
      if (ptr->next_.get() != q.tail_) out << ", ";
    }
    return out;
  }

 private:
  /**
   * Node in the queue, holding the enqueued value.
   */
  struct Node {
    /*** The enqueued value */
    std::shared_ptr<T> data_;

    /*** Pointer to the next node in the queue */
    std::unique_ptr<Node> next_;
  };

  /**
   * Protected get of pointer to tail node.
   */
  Node *get_tail() {
    std::lock_guard<std::mutex> lock(tail_mtx_);
    return tail_;
  }

  /** @overload */
  const Node *get_tail() const {
    std::lock_guard<std::mutex> lock(tail_mtx_);
    return tail_;
  }

  /**
   * Helper function to unlink the front node of the queue.
   *
   * @pre The queue is non-empty and the head mutex is acquired.
   */
  std::unique_ptr<Node> unlink_front() {
    std::unique_ptr<Node> head = std::move(head_);
    head_ = std::move(head->next_);
    --size_;
    assert(head_.get() != nullptr);
    return head;
  }

  /**
   * Pop front node, blocking if one is not available.
   *
   * @internal
   *
   * We use `get_tail()` here to fetch the value of the tail, which
   * means we are releasing the tail mutex after fetching the tail
   * pointer. This means that elements can be added to the queue after
   * we have fetched the tail pointer.
   *
   * However, since we have aquired the head mutex, it is not possible
   * that any elements are *removed* from the queue, which means that
   * if the test for empty was false when we fetched the tail pointer,
   * it will be `false` even if more items are added to the queue
   * while this thread does the comparison.
   */
  std::unique_ptr<Node> pop_front() {
    std::unique_lock<std::mutex> lock(head_mtx_);
    cond_.wait(lock, [this] { return head_.get() != get_tail(); });
    return unlink_front();
  }

  template <class Rep, class Period>
  std::unique_ptr<Node> pop_front(
      const std::chrono::duration<Rep, Period> &rel_time) {
    std::unique_lock<std::mutex> lock(head_mtx_);
    auto not_empty = [this] { return head_.get() != get_tail(); };
    if (cond_.wait_for(lock, rel_time, not_empty)) return unlink_front();
    return std::unique_ptr<Node>();
  }

  /**
   * Pop front node, if one is available.
   */
  std::unique_ptr<Node> try_pop_front() {
    std::lock_guard<std::mutex> lock(head_mtx_);
    if (head_.get() == get_tail()) return std::unique_ptr<Node>();
    return unlink_front();
  }

 public:
  using size_type = std::size_t;

  queue() : size_(0), head_(new Node), tail_(head_.get()) {}

  queue(const queue &) = delete;
  queue &operator=(const queue &) = delete;

  /**
   * Get the size of the queue.
   *
   * @return A positive integer denoting the number of elements in the
   * queue.
   */
  size_type size(std::memory_order order = std::memory_order_seq_cst) const
      noexcept {
    return size_.load(order);
  }

  /**
   * Check if the queue is empty.
   *
   * @note Checking if the queue is empty before removing an element
   * from the queue is not thread safe and in this case `try_pop`
   * should be used intead.
   */
  bool empty() const {
    std::lock_guard<std::mutex> lock(head_mtx_);
    return head_.get() == get_tail();
  }

  void push(T val) {
    // The push works by creating a new empty sentinel node and a
    // shared pointer to the value to push. After that, the value is
    // moved to the current tail node and the new empty sentinel is
    // added last.
    std::unique_ptr<Node> new_node(new Node);
    std::shared_ptr<T> new_data = std::make_shared<T>(std::move(val));

    {
      std::lock_guard<std::mutex> lock(tail_mtx_);
      Node *ptr = new_node.get();
      tail_->data_ = std::move(new_data);
      tail_->next_ = std::move(new_node);
      tail_ = ptr;
      ++size_;
      assert(tail_ != nullptr);
      assert(tail_->data_.get() == nullptr);
      assert(tail_->next_.get() == nullptr);
    }
    cond_.notify_one();
  }

  bool pop(T *result) {
    auto head = pop_front();
    *result = std::move(*head->data_);
    return true;
  }

  template <class Rep, class Period>
  bool pop(T *result, const std::chrono::duration<Rep, Period> &rel_time) {
    if (auto head = pop_front(rel_time)) {
      *result = std::move(*head->data_);
      return true;
    }
    return false;
  }

  std::shared_ptr<T> pop() {
    std::unique_ptr<Node> head = pop_front();
    return head->data_;
  }

  template <class Rep, class Period>
  std::shared_ptr<T> pop(const std::chrono::duration<Rep, Period> &rel_time) {
    if (std::unique_ptr<Node> head = pop_front(rel_time)) return head->data_;
    return std::shared_ptr<T>();
  }

  bool try_pop(T *result) {
    if (std::unique_ptr<Node> head = try_pop_front()) {
      *result = std::move(*head->data_);
      return true;
    }
    return false;
  }

  std::shared_ptr<T> try_pop() {
    if (std::unique_ptr<Node> head = try_pop_front()) {
      return head->data_;
    } else {
      return std::shared_ptr<T>();
    }
  }

 private:
  mutable std::condition_variable cond_;
  std::atomic_size_t size_;
  std::unique_ptr<Node> head_;
  mutable std::mutex head_mtx_;
  Node *tail_;
  mutable std::mutex tail_mtx_;
};

}  // namespace mysql_harness

#endif /* MYSQL_HARNESS_QUEUE_INCLUDED */
