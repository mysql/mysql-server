/* Copyright (c) 2018, 2023, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef GCS_MPSC_QUEUE_INCLUDED
#define GCS_MPSC_QUEUE_INCLUDED

#include <atomic>
#include <cassert>
#include <memory>

/**
 * MPSC queue with FIFO semantics.
 *
 * Implemented as a linked list of nodes.
 * Inspired by Dmitry Vyukov's "non-intrusive MPSC node-based queue" algorithm,
 * available on 2017-07-10 at
 * http://www.1024cores.net/home/lock-free-algorithms/queues/non-intrusive-mpsc-node-based-queue
 */
template <typename T, typename Deleter = std::default_delete<T>>
class Gcs_mpsc_queue {
 private:
  /**
   * Node that holds an element (payload) of the MPSC queue.
   */
  class Gcs_mpsc_queue_node {
   public:
    /**
     * Creates an unlinked and empty node.
     */
    Gcs_mpsc_queue_node() : Gcs_mpsc_queue_node(nullptr) {}
    /**
     * Creates an unlinked node with the given payload.
     * The node takes ownership of @c initial_payload.
     *
     * @param initial_payload the payload
     */
    Gcs_mpsc_queue_node(T *initial_payload) : m_payload(initial_payload) {
      m_next.store(nullptr, std::memory_order_relaxed);
    }
    /* Not copyable nor movable. */
    Gcs_mpsc_queue_node(Gcs_mpsc_queue_node const &) = delete;
    Gcs_mpsc_queue_node(Gcs_mpsc_queue_node &&) = delete;
    Gcs_mpsc_queue_node &operator=(Gcs_mpsc_queue_node const &) = delete;
    Gcs_mpsc_queue_node &operator=(Gcs_mpsc_queue_node &&) = delete;
    /**
     * Gets the next node in the linked list.
     *
     * @param memory_order the desired memory ordering semantics for the load
     * @retval Gcs_mpsc_queue_node* if there is a linked node
     * @retval nullptr otherwise
     */
    Gcs_mpsc_queue_node *get_next(std::memory_order memory_order) {
      return m_next.load(memory_order);
    }
    /**
     * Links a node to this node.
     *
     * @param node the node to link
     * @param memory_order the desired memory ordering semantics for the store
     */
    void set_next(Gcs_mpsc_queue_node *node, std::memory_order memory_order) {
      m_next.store(node, memory_order);
    }
    /**
     * Extracts the payload from this node.
     * Transfers ownership of the payload to the caller, i.e. after calling this
     * method this node has no payload.
     *
     * @retval T* if there is a payload
     * @retval nullptr otherwise
     */
    T *extract_payload() {
      T *result = m_payload;
      m_payload = nullptr;
      return result;
    }

   private:
    /**
     * The next node in the linked list.
     */
    std::atomic<Gcs_mpsc_queue_node *> m_next;
    /**
     * The payload.
     */
    T *m_payload;
  };

 public:
  /**
   * Create an empty queue.
   */
  Gcs_mpsc_queue() : Gcs_mpsc_queue(Deleter()) {}
  Gcs_mpsc_queue(Deleter custom_deleter)
      : m_payload_deleter(custom_deleter), m_tail(new Gcs_mpsc_queue_node()) {
    m_head.store(m_tail, std::memory_order_relaxed);
  }
  /**
   * Destroy the queued nodes.
   */
  ~Gcs_mpsc_queue() {
    // delete the list
    for (T *payload = pop(); payload != nullptr; payload = pop()) {
      m_payload_deleter(payload);
    }
    // delete stub node
    assert(m_tail == m_head.load(std::memory_order_relaxed));
    delete m_tail;
  }
  /* Not copyable nor movable. */
  Gcs_mpsc_queue(Gcs_mpsc_queue const &) = delete;
  Gcs_mpsc_queue(Gcs_mpsc_queue &&) = delete;
  Gcs_mpsc_queue &operator=(Gcs_mpsc_queue const &) = delete;
  Gcs_mpsc_queue &operator=(Gcs_mpsc_queue &&) = delete;
  /**
   * Insert @c payload at the end of the queue.
   *
   * @param payload the element to insert
   * @returns true if the insertion was successful, false otherwise
   */
  bool push(T *payload) {
    bool successful = false;
    auto *new_node = new (std::nothrow) Gcs_mpsc_queue_node(payload);
    if (new_node != nullptr) {
      Gcs_mpsc_queue_node *previous =
          m_head.exchange(new_node, std::memory_order_acq_rel);
      previous->set_next(new_node, std::memory_order_release);
      successful = true;
    }
    return successful;
  }
  /**
   * Attempt to retrieve the first element from the queue.
   *
   * Note that this is a non-blocking method.
   *
   * @retval T* if the queue is not empty
   * @retval nullptr if the queue is empty
   */
  T *pop() {
    T *result = nullptr;
    Gcs_mpsc_queue_node *old_tail = m_tail;
    Gcs_mpsc_queue_node *next_node =
        m_tail->get_next(std::memory_order_acquire);
    if (next_node != nullptr) {
      m_tail = next_node;
      delete old_tail;
      result = m_tail->extract_payload();
    }
    return result;
  }

 private:
  Deleter m_payload_deleter;
  Gcs_mpsc_queue_node *m_tail;                // first in
  std::atomic<Gcs_mpsc_queue_node *> m_head;  // last in
};

#endif /* GCS_MPSC_QUEUE_INCLUDED */
