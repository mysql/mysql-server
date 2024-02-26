/* Copyright (c) 2014, 2023, Oracle and/or its affiliates.

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

#ifndef CHANGESTREAM_APPLY_COMMIT_ORDER_QUEUE
#define CHANGESTREAM_APPLY_COMMIT_ORDER_QUEUE
#include <cstdint>
#include <vector>

#include "sql/containers/integrals_lockfree_queue.h"  // container::Integrals_lockfree_queue
#include "sql/locks/shared_spin_lock.h"               // lock::Shared_spin_lock
#include "sql/mdl.h"                                  // MDL_context
#include "sql/memory/aligned_atomic.h"                // memory::Aligned_atomic

namespace cs {
namespace apply {
/**
  Queue to maintain the ordered sequence of workers waiting for commit.

  The queue has a static list of elements, each one representing each worker
  commit information.

  The management of the order by which each worker will commit is implemented
  using:

  - A member variable pointing to the first worker to commit, the `head`.
  - A member variable pointing to the last worker to commit, the `tail`.
  - Each queue element holds a member variable that points to the next worker to
    commit, the `next`.
  - Pushing a new element will move the `tail`.
  - Popping an element will move the `head`.

  Atomics are used to make the queue thread-safe without the need for an
  explicit lock.
 */
class Commit_order_queue {
 public:
  using value_type = unsigned long;
  using queue_type = container::Integrals_lockfree_queue<value_type>;
  using sequence_type = unsigned long long;
  static constexpr value_type NO_WORKER{
      queue_type::null_value};  // No worker on the queue

  /**
  Enumeration to represent each worker state
 */
  enum class enum_worker_stage {
    REGISTERED,         // Transaction was handed-over to worker, for applying
    FINISHED_APPLYING,  // Transaction execution finished
    REQUESTED_GRANT,    // Request for turn to commit has been placed
    WAITED,             // Waited for the turn to commit
    FINISHED            // Committed and finished processing the transaction
  };

  /**
    Queue element, holding the needed information to manage the commit ordering.
   */
  class Node {
   public:
    friend class Commit_order_queue;

    /** The identifier of the worker that maps to a queue index. */
    value_type m_worker_id{NO_WORKER};
    /** The MDL context to be used to wait on the MDL graph. */
    MDL_context *m_mdl_context{nullptr};
    /** Which stage is the worker on. */
    memory::Aligned_atomic<Commit_order_queue::enum_worker_stage> m_stage{
        Commit_order_queue::enum_worker_stage::FINISHED};

    /**
      Marks the commit request sequence number this node's worker is
      processing as frozen iff the sequence number current value is equal
      to the `expected` parameter.

      Commit request sequence numbers are monotonically ever increasing
      numbers that are used by worker threads to ensure ownership of the
      worker commit turn unblocking operation:
      1) A worker holding a sequence number `N` can only unblock worker
         with sequence number `N + 1`.
      2) A worker with sequence number `N + 1` can't be assigned a new
         sequence number if worker with sequence number `N` is executing
         the unblocking operation.

      @param expected the commit request sequence number this node must
                      currently hold in order for it to be frozen

      @return true if this nodes commit request sequence number has been
              frozen, false otherwise.
     */
    bool freeze_commit_sequence_nr(Commit_order_queue::sequence_type expected);
    /**
      Removes the frozen mark from the commit request sequence number this
      node's worker is processing if it was previously frozen.

      Commit request sequence numbers are monotonically ever increasing
      numbers that are used by worker threads to ensure ownership of the
      worker commit turn unblocking operation:
      1) A worker holding a sequence number `N` can only unblock worker
         with sequence number `N + 1`.
      2) A worker with sequence number `N + 1` can't be assigned a new
         sequence number if worker with sequence number `N` is executing
         the unblocking operation.

      @param previously_frozen the sequence number value that was provided
                               while previously freezing.

      @return true if this nodes commit request sequence number was frozen
              and is now unfrozen, false otherwise.
     */
    bool unfreeze_commit_sequence_nr(
        Commit_order_queue::sequence_type previously_frozen);

   private:
    // No commit request sequence number assigned
    static constexpr Commit_order_queue::sequence_type NO_SEQUENCE_NR{0};
    // Commit request sequence number is marked as frozen
    static constexpr Commit_order_queue::sequence_type SEQUENCE_NR_FROZEN{1};

    /** The sequence number for the commit request this node's worker is
        processing. */
    memory::Aligned_atomic<Commit_order_queue::sequence_type>
        m_commit_sequence_nr{NO_SEQUENCE_NR};

    /**
      Sets the commit request sequence number for this node as unassigned. If
      the sequence number is currently frozen, invoking this method will make
      the invoking thread to spin until the sequence number is unfrozen.

      Commit request sequence numbers are monotonically ever increasing
      numbers that are used by worker threads to ensure ownership of the
      worker commit turn unblocking operation:
      1) A worker holding a sequence number `N` can only unblock worker
         with sequence number `N + 1`.
      2) A worker with sequence number `N + 1` can't be assigned a new
         sequence number if worker with sequence number `N` is executing
         the unblocking operation.

      @return the sequence number for the commit request sequence number
              this node's worker is been cleared of
     */
    Commit_order_queue::sequence_type reset_commit_sequence_nr();
  };

  /**
    Iterator helper class to iterate over the Commit_order_queue following the
    underlying commit order.


    Check C++ documentation on `Iterator named requirements` for more
    information on the implementation.
   */
  class Iterator {
   public:
    using difference_type = std::ptrdiff_t;
    using value_type = Commit_order_queue::Node *;
    using pointer = Commit_order_queue::Node *;
    using reference = Commit_order_queue::Node *;
    using iterator_category = std::forward_iterator_tag;
    using index_type = Commit_order_queue::queue_type::index_type;

    explicit Iterator(Commit_order_queue &parent, index_type position);
    Iterator(const Iterator &rhs);
    Iterator(Iterator &&rhs);
    virtual ~Iterator() = default;

    // BASIC ITERATOR METHODS //
    Iterator &operator=(const Iterator &rhs);
    Iterator &operator=(Iterator &&rhs);
    Iterator &operator++();
    reference operator*();
    // END / BASIC ITERATOR METHODS //

    // INPUT ITERATOR METHODS //
    Iterator operator++(int);
    pointer operator->();
    bool operator==(Iterator const &rhs) const;
    bool operator!=(Iterator const &rhs) const;
    // END / INPUT ITERATOR METHODS //

    // OUTPUT ITERATOR METHODS //
    // reference operator*(); <- already defined
    // iterator operator++(int); <- already defined
    // END / OUTPUT ITERATOR METHODS //

    // FORWARD ITERATOR METHODS //
    // Enable support for both input and output iterator <- already enabled
    // END / FORWARD ITERATOR METHODS //

   private:
    /** The target queue that holds the list to be iterated. */
    Commit_order_queue *m_target{nullptr};
    /** The iterator pointing to the underlying queue position. */
    Commit_order_queue::queue_type::Iterator m_current;
  };

  /**
    Constructor for the class, takes the number of workers and initializes the
    underlying static list with such size.

    @param n_workers The number of workers to include in the commit order
                     managerment.
   */
  Commit_order_queue(size_t n_workers);
  /**
    Default destructor for the class.
   */
  virtual ~Commit_order_queue() = default;
  /**
    Retrieve the commit order information Node for worker identified by `id`.

    @param id The identifier of the worker

    @return A reference to the commit order information Node for the given
            worker.
   */
  Node &operator[](value_type id);
  /**
    Retrieves the error state for the current thread last executed queue
    operation. Values may be:

    - SUCCESS is the operation succeeded.
    - NO_MORE_ELEMENTS if the last pop tried to access an empty queue.
    - NO_SPACE_AVAILABLE if the last push tried to push while the queue was
      full.


    @return The error state for the thread's last operation on the queue.
   */
  Commit_order_queue::queue_type::enum_queue_state get_state();
  /**
    Whether or not there are more workers to commit.

    @return True if there are no more workers, false otherwise.
   */
  bool is_empty();
  /**
    Removes from the queue and returns the identifier of the worker that is
    first in-line to commit.

    If another thread is accessing the commit order sequence number and has
    frozen it's state, this operation will spin until the state is
    unfrozen.

    @return A tuple holding the identifier of the worker that is first
            in-line to commit and the associated commit order sequence
            number.
   */
  std::tuple<value_type, sequence_type> pop();
  /**
    Adds to the end of the commit queue the worker identifier passed as
    parameter.

    @param id The identifier of the worker to add to the commit queue.
   */
  void push(value_type id);
  /**
    Retrieves the identifier of the worker that is first in-line to commit.

    @return The identifier of the worker that is first in-line to commit.
   */
  value_type front();
  /**
    Removes all remaining workers from the queue.
   */
  void clear();
  /**
    Acquires exclusivity over changes (push, pop) on the queue.
   */
  void freeze();
  /**
    Releases exclusivity over changes (push, pop) on the queue.
   */
  void unfreeze();
  /**
    Retrieves an iterator instance that points to the head of the commit queue
    and that will iterate over the worker Nodes that are in-line to commit,
    following the requested commit order.

    @return An instance of `Iterator` pointing to the queue's head, that
            iterates over the workers that are in-line to commit and following
            the requested commit order.
   */
  Iterator begin();
  /**
    Retrieves an iterator instance that points to the tail of the commit queue.

    @return An instance of `Iterator` that points to the tail of the
    queue.
   */
  Iterator end();
  /**
    Retrieves the textual representation of this object's underlying commit
    queue.

    @return The textual representation of this object's underlying commit queue.
   */
  std::string to_string();
  /**
    Friend operator for writing to an `std::ostream` object.

    @see `std::ostream::operator<<`
   */
  friend inline std::ostream &operator<<(std::ostream &out,
                                         Commit_order_queue &to_output) {
    out << to_output.to_string() << std::flush;
    return out;
  }

  /**
    Returns the expected next number in the ticket sequence.

    @param current_seq_nr The current sequence number, for which the next
                          should be computed.

    @return The expected next number in the ticket sequence.
   */
  static sequence_type get_next_sequence_nr(sequence_type current_seq_nr);

 private:
  /** The commit sequence number counter */
  memory::Aligned_atomic<sequence_type> m_commit_sequence_generator{
      Node::SEQUENCE_NR_FROZEN + 1};
  /** The list of worker Nodes, indexed by worker ID. */
  std::vector<Commit_order_queue::Node> m_workers;
  /** The queue to hold the sequence of worker IDs waiting to commit. */
  queue_type m_commit_queue;
  /** The lock to acquire exlusivity over changes on the queue. */
  lock::Shared_spin_lock m_push_pop_lock;
};
}  // namespace apply
}  // namespace cs
#endif  // CHANGESTREAM_APPLY_COMMIT_ORDER_QUEUE
