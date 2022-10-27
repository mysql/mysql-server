/* Copyright (c) 2020, 2022, Oracle and/or its affiliates.

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

#ifndef CONTAINER_INTEGRALS_LOCKFREE_QUEUE_INCLUDED
#define CONTAINER_INTEGRALS_LOCKFREE_QUEUE_INCLUDED

#include <algorithm>
#include <atomic>
#include <cmath>
#include <map>
#include <memory>
#include <sstream>
#include <thread>
#include <tuple>

#include "sql/containers/atomics_array.h"
#include "sql/memory/aligned_atomic.h"
#include "sql/memory/unique_ptr.h"

namespace container {

/**
  Lock-free, fixed-size bounded, multiple-producer (MP), multiple-consumer
  (MC), circular FIFO queue for integral elements.

  Monotonically ever increasing virtual indexes are used to keep track of
  the head and tail pointer for the size-bounded circular queue. Virtual
  indexes are translated into memory indexes by calculating the remainder
  of the integer division of the virtual index by the queue capacity. The
  maximum value of a virtual index is 2^63 - 1.

  Head is the pointer to the virtual index of the first position that holds
  an element to be popped, if any.

  Tail is the pointer to the virtual index of the first available position
  to push an element to, if any.

  Template parameters are as follows:
  - `T`: The integral type for the queue elements.
  - `Null`: Value of type `T`, that will be used to mark a queue position
            as empty.
  - `Erased`: Value of type `T`, that will be used to mark an queue
              position as erased.
  - `I`: Type of indexing to be used by the underlying array in the form of
         a class. Available classes are `container::Padded_indexing` and
         `container::Interleaved_indexing`, check the classes documentation
         for further details. The parameter defaults to
         `container::Padded_indexing`.
  - `A`: Type of memory allocator to be used, in the form of a class
         (defaults to no allocator).

  All the available operations are thread-safe, in the strict sense of no
  memory problems rise from multiple threads trying to perform operations
  concurrently.

  However, being a lock-free structure, the queue may be changing at the
  same time as operations access both pointers and values or a client of
  the API evaluates the result of the invoked operation. The operation
  results and returning states are always based on the thread local view of
  the queue state, which may be safe or unsafe to proceed with the given
  operation. Therefore, extra validations, client-side serialization and/or
  retry mechanisms may be needed while using the queue operations.

  Available methods are:
  - `pop`: if the head of the queue points to a virtual index different
           from the one pointed by the tail of the queue, removes the
           element pointed to by the head of the queue, points the head to
           the next virtual index and returns the retrieved value. If head
           and tail of the queue point to the same virtual index, the queue
           is empty, `Null` is returned and the thread operation state is
           set to `NO_MORE_ELEMENTS`.
  - `push`: if the tail of the queue points to a virtual index different
            from the one pointed by head-plus-queue-capacity, sets the
            position pointed by tail with the provided element and points
            the tail to the next virtual index. If tail and head plus queue
            capacity point to the same virtual index, the queue is full, no
            operation is performed and the thread operation state is set to
            `NO_SPACE_AVAILABLE`.
  - `capacity`: maximum number of elements allowed to coexist in the queue.
  - `head`: pointer to the virtual index of the first available element, if
            any.
  - `tail`: pointer to the virtual index of the first available position to
            add an element to. Additionally, the value returned by `tail()`
            can also give an lower-than approximation of the total amount
            of elements already pushed into the queue -in between reading
            the virtual index tail is pointing to and evaluating it, some
            more elements may have been pushed.
  - `front`: the first available element. If none, `Null` is returned.
  - `back`: the last available element. If none, `Null` is returned.
  - `clear`: sets all positions of the underlying array to `Null` and
             resets the virtual index pointed to by both the head and the
             tail of the queue to `0`.
  - `is_empty`: whether or not the head and tail of the queue point to the
                same virtual index.
  - `is_full`: whether or not the tail and head-plus-queue-capacity point
               to the same virtual index.
  - `erase_if`: traverses the queue, starting at the queue relative memory
                index 0, stopping at the relative memory index
                `capacity() - 1` and invoking the passed-on predicate over each
                position value, while disregarding positions that have
                `Null` or `Erased` values.

  Note that, if `Null` and `Erased` hold the same value, the resulting class
        will not include the `erase_if` method.

 */
template <typename T, T Null = std::numeric_limits<T>::max(), T Erased = Null,
          typename I = container::Padded_indexing<T>,
          typename A = std::nullptr_t>
class Integrals_lockfree_queue {
  static_assert(
      std::is_integral<T>::value,
      "class `Integrals_lockfree_queue` requires an integral type as a first "
      "template parameter");

 public:
  using pointer_type = T *;
  using const_pointer_type = T const *;
  using reference_type = T &;
  using const_reference_type = T const &;
  using value_type = T;
  using const_value_type = T const;
  using element_type = std::atomic<T>;
  using index_type = unsigned long long;
  using array_type = container::Atomics_array<T, I, A>;
  using atomic_type = memory::Aligned_atomic<index_type>;

  static constexpr T null_value = Null;
  static constexpr T erased_value = Erased;
  static constexpr index_type set_bit = 1ULL << 63;
  static constexpr index_type clear_bit = set_bit - 1;

  enum class enum_queue_state : short {
    SUCCESS = 0,             // Last operation was successful
    NO_MORE_ELEMENTS = -1,   // Last operation was unsuccessful because there
                             // are no elements in the queue
    NO_SPACE_AVAILABLE = -2  // Last operation was unsuccessful because there
                             // is no space available
  };

  /**
    Iterator helper class to iterate over the queue staring at the virtual
    index pointed to by the head, up until the virtual index pointed to by
    the tail.

    Being an iterator over a lock-free structure, it will not be
    invalidated upon queue changes since operations are thread-safe and no
    invalid memory access should stem from iterating over and changing the
    queue, simultaneously.

    However, the following possible iteration scenarios, not common in
    non-thread-safe structures, should be taken into consideration and
    analysed while using standard library features that use the `Iterator`
    requirement, like `for_each` loops, `std::find`, `std::find_if`, etc:

    a) The iteration never stops because there is always an element being
       pushed before the queue `end()` method is invoked.

    b) The iterator points to `Null` values at the beginning of the
       iteration because elements were popped just after the queue `begin()`
       method is invoked.

    c) The iterator points to `Null` or `Erased` in between pointing to
       values different from `Null` or `Erased`.

    d) The iterator may point to values that do not correspond to the
       virtual index being held by the iterator, in the case of the amount
       of both pop and push operations between two iteration loops was
       higher then the queue `capacity()`.

    If one of the above scenarios is harmful to your use-case, an
    additional serialization mechanism may be needed to iterate over the
    queue. Or another type of structure may be more adequate.

    Check C++ documentation for the definition of `Iterator` named
    requirement for more information.
   */
  class Iterator {
   public:
    using difference_type = std::ptrdiff_t;
    using value_type = T;
    using pointer = T *;
    using reference = T;
    using iterator_category = std::forward_iterator_tag;

    explicit Iterator(
        Integrals_lockfree_queue<T, Null, Erased, I, A> const &parent,
        index_type position);
    Iterator(const Iterator &rhs);
    Iterator(Iterator &&rhs);
    virtual ~Iterator() = default;

    // BASIC ITERATOR METHODS //
    Iterator &operator=(const Iterator &rhs);
    Iterator &operator=(Iterator &&rhs);
    Iterator &operator++();
    reference operator*() const;
    // END / BASIC ITERATOR METHODS //

    // INPUT ITERATOR METHODS //
    Iterator operator++(int);
    pointer operator->() const;
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

    /**
      Sets the value of the element the iterator is pointing to the given
      parameter.

      @param new_value The new value to set the element to.
     */
    void set(value_type new_value);

   private:
    /** The position of the element this iterator is pointing to. */
    index_type m_current{std::numeric_limits<index_type>::max()};
    /** The reference to the queue holding the elements. */
    Integrals_lockfree_queue<T, Null, Erased, I, A> const *m_parent{nullptr};
  };

  /**
    Constructor allowing a specific memory allocator and a specific queue
    capacity.

    The queue allocated memory may differ from `capacity() * sizeof(T)`
    since additional space may be required to prevent false sharing between
    threads.

    @param alloc The memory allocator instance
    @param size The queue maximum capacity
   */
  template <
      typename D = T, T M = Null, T F = Erased, typename J = I, typename B = A,
      std::enable_if_t<!std::is_same<B, std::nullptr_t>::value> * = nullptr>
  Integrals_lockfree_queue(A &alloc, size_t size);
  /**
    Constructor allowing specific queue capacity.

    The queue allocated memory may differ from `capacity() * sizeof(T)`
    since additional space may be required to prevent false sharing between
    threads.

    @param size The queue maximum capacity
   */
  template <
      typename D = T, T M = Null, T F = Erased, typename J = I, typename B = A,
      std::enable_if_t<std::is_same<B, std::nullptr_t>::value> * = nullptr>
  Integrals_lockfree_queue(size_t size);
  // Deleted copy and move constructors.
  Integrals_lockfree_queue(
      Integrals_lockfree_queue<T, Null, Erased, I, A> const &rhs) = delete;
  Integrals_lockfree_queue(
      Integrals_lockfree_queue<T, Null, Erased, I, A> &&rhs) = delete;
  //
  /**
    Destructor for the class.
   */
  virtual ~Integrals_lockfree_queue() = default;
  // Deleted copy and move operators.
  Integrals_lockfree_queue<T, Null, Erased, I, A> &operator=(
      Integrals_lockfree_queue<T, Null, Erased, I, A> const &rhs) = delete;
  Integrals_lockfree_queue<T, Null, Erased, I, A> &operator=(
      Integrals_lockfree_queue<T, Null, Erased, I, A> &&rhs) = delete;
  //

  /**
    Returns the underlying instance of `memory::Atomics_array` which holds
    the allocated memory for the array of `std::atomic<T>` elements.

    @return the underlying instance of `memory::Atomics_array`
   */
  array_type &array();
  /**
    Sets all queue positions to `Null` and points the head and tail of the
    queue to the `0` virtual index.
   */
  void clear();
  /**
    Retrieves whether or not the head and tail of the queue are pointing to
    the same virtual index.

    No evaluation of the value held in the given position is made. If, for
    instance, head and tail point to consecutive virtual indexes and the
    value stored in the position pointed to by head is `Erased`, `is_empty`
    will return false and `pop` will return `Null` and trigger a
    `NO_MORE_ELEMENTS` state change.

    @return true if there aren't more elements to be popped, false otherwise
   */
  bool is_empty() const;
  /**
    Retrieves whether or not the tail of the queue is pointing to the same
    virtual index as the computed by adding the queue `capacity()` to the
    virtual index pointed to by the head of the queue.

    No evaluation of the value held in the given position is made. If, for
    instance, all the values stored in the positions between head and tail
    are `Erased`, `is_full` will return true and `pop` will return `Null`
    and trigger a `NO_MORE_ELEMENTS` state change.

    @return true if there isn't space for more elements to be pushed, false
            otherwise
   */
  bool is_full() const;
  /**
    Retrieves the virtual index that the head of the queue is pointing to.

    @return the virtual index the head of the queue is pointing to
   */
  index_type head() const;
  /**
    Retrieves the virtual index that the tail of the queue is pointing to.

    @return the virtual index the tail of the queue is pointing to
   */
  index_type tail() const;
  /**
    Retrieves the element at the front of the queue, i.e. the value stored
    in the virtual index pointed to by the head of the queue.

    The returned value may be `Null`, `Erased` or whatever value that is
    held by the given virtual index position at the moment it's accessed.

    As this method is an alias for `array()[head()]`, the queue may be
    changed concurrently and, because it is a circular queue, it is
    possible for this method to return a value that has not been popped yet
    and it will not be popped in the next call for `pop()` (the circular
    queue logic made the tail to wrap and overlap the thread local value of
    head).

    @return the element at the front of the queue
   */
  value_type front() const;
  /**
    Retrieves the value of the position at the back of the queue, i.e. the
    value stored in the virtual index just prior to the virtual index
    pointed to by the tail of the queue.

    The returned value may be `Null` or `Erased`, whatever value that is
    held by the given virtual index position.

    As this method is an alias for `array()[tail()]`, the queue may be
    changed concurrently and it is possible for this method to return a
    value assigned to a position outside the bounds of the head and tail of
    the queue (between thread-local fetch of the tail pointer and the
    access to the position indexed by the local value, operations moved the
    head to a virtual index higher than the locally stored). This means
    that `Null` may be returned or that a value that is currently being
    popped may be returned.

    @return the element at the back of the queue
   */
  value_type back() const;
  /**
    Retrieves the value at the virtual index pointed by the head of the
    queue, clears that position, updates the virtual index stored in the
    head and clears the value returned by `get_state()`, setting it to
    `SUCCESS`.

    If the head of the queue points to a virtual index that has no element
    assigned (queue is empty), the operation fails, `Null` is stored in the
    `out` parameter and the value returned by `get_state()` is
    `NO_MORE_ELEMENTS`.

    @param out The variable reference to store the value in.

    @return The reference to `this` object, for chaining purposes.
   */
  Integrals_lockfree_queue<T, Null, Erased, I, A> &operator>>(
      reference_type out);
  /**
    Takes the value passed on as a parameter, stores it in the virtual
    index pointed to by the tail of the queue, updates the virtual index
    stored in the tail and clears the value returned by `get_state()`,
    setting it to `SUCCESS`.

    If the tail of the queue points to a virtual index that has already an
    element assigned (queue is full), the operation fails and the value
    returned by `get_state()` is `NO_SPACE AVAILABLE`.

    @param to_push The value to push into the queue.

    @return The reference to `this` object, for chaining purposes.
   */
  Integrals_lockfree_queue<T, Null, Erased, I, A> &operator<<(
      const_reference_type to_push);
  /**
    Retrieves the value at the virtual index pointed by the head of the
    queue, clears that position, updates the virtual index stored in the
    head and clears the value returned by `get_state()`, setting it to
    `SUCCESS`.

    If the head of the queue points to a virtual index that has no element
    assigned yet (queue is empty), the operation returns `Null` and the
    value returned by `get_state()` is `NO_MORE_ELEMENTS`.

    @return The value retrieved from the queue or `Null` if no element is
            available for popping
   */
  value_type pop();
  /**
    Takes the value passed on as a parameter, stores it in the virtual
    index pointed to by the tail of the queue, updates the virtual index
    stored in the tail and clears the value returned by `get_state()`,
    setting it to `SUCCESS`.

    If the tail of the queue points to a virtual index that has already an
    element assigned (queue is full), the operation fails and the value
    returned by `get_state()` is `NO_SPACE AVAILABLE`.

    @param to_push The value to push into the queue.

    @return The reference to `this` object, for chaining purposes.
   */
  Integrals_lockfree_queue<T, Null, Erased, I, A> &push(value_type to_push);
  /**
    Retrieves an iterator instance that points to the same position pointed
    by the head of the queue.

    Please, be aware that, while using standard library features that use
    the `Iterator` requirement, like `for_each` loops, `std::find`,
    `std::find_if`, etc:

    - The iterator may point to `Null` values at the beginning of the
      iteration because elements were popped just after the queue `begin()`
      method is invoked, the moment when the iterator pointing to the same
      virtual index as the head of the queue is computed.

    @return An instance of an iterator instance that points to virtual
            index pointed by the head of the queue
   */
  Iterator begin() const;
  /**
    Retrieves an iterator instance that points to the same position pointed
    by the tail of the queue.

    Please, be aware that, while using standard library features that use
    the `Iterator` requirement, like `for_each` loops, `std::find`,
    `std::find_if`, etc:

    - The iteration may never stop because there is always an element being
      pushed before the queue `end()` method is invoked, the moment when
      the iterator pointing to the same virtual index has the tail of the
      queue is computed.

    @return An instance of an iterator instance that points to virtual
            index pointed by the tail of the queue
   */
  Iterator end() const;
  /**
    Erases values from the queue. The traversing is linear and not in
    between the virtual indexes pointed to by the head and the tail of the
    queue but rather between 0 and `Integrals_lockfree_queue::capacity() -
    1`.

    An element may be conditionally erased according to the evaluation of
    the predicate `predicate` which should be any predicate which is
    translatable to `[](value_type value) -> bool`. If the predicate
    evaluates to `true`, the value is replace by `Erased`.

    If both `Null` and `Erased` evaluate to the same value, this method
    will not be available after the template substitutions since erased
    values must be identifiable by the pop and push operations.

    Check C++ documentation for the definition of `Predicate` named
    requirement for more information.

    @param predicate The predicate invoked upon a given queue position and
                     if evaluated to `true` will force the removal of such
                     element.

    @return The number of values erased.
   */
  template <typename D = T, T M = Null, T F = Erased, typename J = I,
            typename B = A, typename Pred, std::enable_if_t<M != F> * = nullptr>
  size_t erase_if(Pred predicate);
  /**
    Returns the maximum number of elements allowed to coexist in the queue.

    @return The maximum number of elements allowed to coexist in the queue
   */
  size_t capacity() const;
  /**
    Returns the amount of bytes needed to store the maximum number of
    elements allowed to coexist in the queue.

    @return the amount of bytes needed to store the maximum number of
            elements allowed to coexist in th queue
   */
  size_t allocated_size() const;
  /**
    Clears the error state of the last performed operations, if any. The
    operation state is a thread storage duration variable, making it a
    per-thread state.

    @return The reference to `this` object, for chaining purposes.
   */
  Integrals_lockfree_queue<T, Null, Erased, I, A> &clear_state();
  /**
    Retrieves the error/success state of the last performed operation. The
    operation state is a thread storage duration variable, making it a
    per-thread state.

    Possible values are:
    - `SUCCESS` if the operation was successful
    - `NO_MORE_ELEMENTS` if there are no more elements to pop
    - `NO_SPACE_AVAILABLE` if there is no more room for pushing elements

    State may be changed by any of the `pop`, `push` operations.

    @return the error/success state of the invoking thread last operation.
   */
  enum_queue_state get_state() const;
  /**
    Return `this` queue textual representation.

    @return the textual representation for `this` queue.
   */
  std::string to_string() const;

  friend std::ostream &operator<<(
      std::ostream &out,
      Integrals_lockfree_queue<T, Null, Erased, I, A> const &in) {
    out << in.to_string() << std::flush;
    return out;
  }

 private:
  /** The maximum allowed number of element allowed to coexist in the queue */
  size_t m_capacity{0};
  /** The array of atomics in which the elements will be stored */
  array_type m_array;
  /** The virtual index being pointed to by the head of the queue */
  atomic_type m_head{0};
  /** The virtual index being pointed to by the tail of the queue */
  atomic_type m_tail{0};

  /**
   Translates a virtual monotonically increasing index into an index bounded to
   the queue capacity.
   */
  size_t translate(index_type from) const;
  /**
    Retrieves the thread storage duration operation state variable.
   */
  enum_queue_state &state() const;
};
}  // namespace container

#ifndef IN_DOXYGEN  // Doxygen doesn't understand this construction.
template <typename T, T Null, T Erased, typename I, typename A>
container::Integrals_lockfree_queue<T, Null, Erased, I, A>::Iterator::Iterator(
    Integrals_lockfree_queue<T, Null, Erased, I, A> const &parent,
    Integrals_lockfree_queue<T, Null, Erased, I, A>::index_type position)
    : m_current{position}, m_parent{&parent} {}
#endif

template <typename T, T Null, T Erased, typename I, typename A>
container::Integrals_lockfree_queue<T, Null, Erased, I, A>::Iterator::Iterator(
    const Iterator &rhs)
    : m_current{rhs.m_current}, m_parent{rhs.m_parent} {}

template <typename T, T Null, T Erased, typename I, typename A>
container::Integrals_lockfree_queue<T, Null, Erased, I, A>::Iterator::Iterator(
    Iterator &&rhs)
    : m_current{rhs.m_current}, m_parent{rhs.m_parent} {
  rhs.m_current = std::numeric_limits<index_type>::max();
  rhs.m_parent = nullptr;
}

template <typename T, T Null, T Erased, typename I, typename A>
typename container::Integrals_lockfree_queue<T, Null, Erased, I, A>::Iterator &
container::Integrals_lockfree_queue<T, Null, Erased, I, A>::Iterator::operator=(
    const Iterator &rhs) {
  this->m_current = rhs.m_current;
  this->m_parent = rhs.m_parent;
  return (*this);
}

template <typename T, T Null, T Erased, typename I, typename A>
typename container::Integrals_lockfree_queue<T, Null, Erased, I, A>::Iterator &
container::Integrals_lockfree_queue<T, Null, Erased, I, A>::Iterator::operator=(
    Iterator &&rhs) {
  this->m_current = rhs.m_current;
  this->m_parent = rhs.m_parent;
  rhs.m_current = std::numeric_limits<index_type>::max();
  rhs.m_parent = nullptr;
  return (*this);
}

template <typename T, T Null, T Erased, typename I, typename A>
typename container::Integrals_lockfree_queue<T, Null, Erased, I, A>::Iterator &
container::Integrals_lockfree_queue<T, Null, Erased, I,
                                    A>::Iterator::operator++() {
  ++this->m_current;
  return (*this);
}

template <typename T, T Null, T Erased, typename I, typename A>
typename container::Integrals_lockfree_queue<T, Null, Erased, I,
                                             A>::Iterator::reference
container::Integrals_lockfree_queue<T, Null, Erased, I,
                                    A>::Iterator::operator*() const {
  return this->m_parent->m_array[this->m_parent->translate(this->m_current)];
}

template <typename T, T Null, T Erased, typename I, typename A>
typename container::Integrals_lockfree_queue<T, Null, Erased, I, A>::Iterator
container::Integrals_lockfree_queue<T, Null, Erased, I,
                                    A>::Iterator::operator++(int) {
  auto to_return = (*this);
  ++(*this);
  return to_return;
}

template <typename T, T Null, T Erased, typename I, typename A>
typename container::Integrals_lockfree_queue<T, Null, Erased, I,
                                             A>::Iterator::pointer
container::Integrals_lockfree_queue<T, Null, Erased, I,
                                    A>::Iterator::operator->() const {
  return &(this->m_parent->m_array[this->m_parent->translate(this->m_current)]);
}

template <typename T, T Null, T Erased, typename I, typename A>
bool container::Integrals_lockfree_queue<
    T, Null, Erased, I, A>::Iterator::operator==(Iterator const &rhs) const {
  return this->m_current == rhs.m_current && this->m_parent == rhs.m_parent;
}

template <typename T, T Null, T Erased, typename I, typename A>
bool container::Integrals_lockfree_queue<
    T, Null, Erased, I, A>::Iterator::operator!=(Iterator const &rhs) const {
  return !((*this) == rhs);
}

template <typename T, T Null, T Erased, typename I, typename A>
void container::Integrals_lockfree_queue<T, Null, Erased, I, A>::Iterator::set(
    value_type new_value) {
  this->m_parent->m_array[this->m_parent->translate(this->m_current)].store(
      new_value);
}

template <typename T, T Null, T Erased, typename I, typename A>
template <typename D, T M, T F, typename J, typename B,
          std::enable_if_t<!std::is_same<B, std::nullptr_t>::value> *>
container::Integrals_lockfree_queue<T, Null, Erased, I,
                                    A>::Integrals_lockfree_queue(A &alloc,
                                                                 size_t size)
    : m_capacity{size},
      m_array{alloc, size, null_value},
      m_head{0},
      m_tail{0} {}

template <typename T, T Null, T Erased, typename I, typename A>
template <typename D, T M, T F, typename J, typename B,
          std::enable_if_t<std::is_same<B, std::nullptr_t>::value> *>
container::Integrals_lockfree_queue<T, Null, Erased, I,
                                    A>::Integrals_lockfree_queue(size_t size)
    : m_capacity{size}, m_array{size, null_value}, m_head{0}, m_tail{0} {}

template <typename T, T Null, T Erased, typename I, typename A>
typename container::Integrals_lockfree_queue<T, Null, Erased, I,
                                             A>::array_type &
container::Integrals_lockfree_queue<T, Null, Erased, I, A>::array() {
  return this->m_array;
}

template <typename T, T Null, T Erased, typename I, typename A>
void container::Integrals_lockfree_queue<T, Null, Erased, I, A>::clear() {
  this->clear_state();
  for (size_t idx = 0; idx != this->m_array.size(); ++idx)
    this->m_array[idx].store(Null);
  this->m_head->store(0);
  this->m_tail->store(0);
}

template <typename T, T Null, T Erased, typename I, typename A>
bool container::Integrals_lockfree_queue<T, Null, Erased, I, A>::is_empty()
    const {
  auto head = this->m_head->load(std::memory_order_acquire) & clear_bit;
  auto tail = this->m_tail->load(std::memory_order_acquire) & clear_bit;
  return head == tail;
}

template <typename T, T Null, T Erased, typename I, typename A>
bool container::Integrals_lockfree_queue<T, Null, Erased, I, A>::is_full()
    const {
  auto tail = this->m_tail->load(std::memory_order_acquire) & clear_bit;
  auto head = this->m_head->load(std::memory_order_acquire) & clear_bit;
  return tail == head + this->capacity();
}

template <typename T, T Null, T Erased, typename I, typename A>
typename container::Integrals_lockfree_queue<T, Null, Erased, I, A>::index_type
container::Integrals_lockfree_queue<T, Null, Erased, I, A>::head() const {
  return this->m_head->load(std::memory_order_seq_cst) & clear_bit;
}

template <typename T, T Null, T Erased, typename I, typename A>
typename container::Integrals_lockfree_queue<T, Null, Erased, I, A>::index_type
container::Integrals_lockfree_queue<T, Null, Erased, I, A>::tail() const {
  return this->m_tail->load(std::memory_order_seq_cst) & clear_bit;
}

template <typename T, T Null, T Erased, typename I, typename A>
typename container::Integrals_lockfree_queue<T, Null, Erased, I, A>::value_type
container::Integrals_lockfree_queue<T, Null, Erased, I, A>::front() const {
  auto head = this->head();
  auto to_return =
      this->m_array[this->translate(head)].load(std::memory_order_seq_cst);
  return to_return == Erased ? Null : to_return;
}

template <typename T, T Null, T Erased, typename I, typename A>
typename container::Integrals_lockfree_queue<T, Null, Erased, I, A>::value_type
container::Integrals_lockfree_queue<T, Null, Erased, I, A>::back() const {
  auto tail = this->tail();
  if (tail == 0) return Null;
  auto to_return =
      this->m_array[this->translate(tail - 1)].load(std::memory_order_seq_cst);
  return to_return == Erased ? Null : to_return;
}

template <typename T, T Null, T Erased, typename I, typename A>
container::Integrals_lockfree_queue<T, Null, Erased, I, A>
    &container::Integrals_lockfree_queue<T, Null, Erased, I, A>::operator>>(
        reference_type out) {
  out = this->pop();
  return (*this);
}

template <typename T, T Null, T Erased, typename I, typename A>
container::Integrals_lockfree_queue<T, Null, Erased, I, A>
    &container::Integrals_lockfree_queue<T, Null, Erased, I, A>::operator<<(
        const_reference_type to_push) {
  return this->push(to_push);
}

template <typename T, T Null, T Erased, typename I, typename A>
T container::Integrals_lockfree_queue<T, Null, Erased, I, A>::pop() {
  this->clear_state();
  for (; true;) {
    auto head = this->m_head->load(std::memory_order_acquire) & clear_bit;
    auto tail = this->m_tail->load(std::memory_order_relaxed) & clear_bit;

    if (head == tail) {
      this->state() = enum_queue_state::NO_MORE_ELEMENTS;
      break;
    }

    auto new_head = head + 1;
    new_head |= set_bit;  // Set the occupied bit
    if (this->m_head->compare_exchange_strong(
            head, new_head,  // Concurrent pop may have reached the CAS first
                             // or the occupied bit hasn't been unset yet
            std::memory_order_release)) {
      auto &current = this->m_array[this->translate(head)];

      for (; true;) {
        value_type value = current.load();
        if (value != Null &&  // It may be `Null` if some concurrent push
                              // operation hasn't finished setting the
                              // element value
            current.compare_exchange_strong(
                value, Null,  // It may have been set to `Erased` concurrently
                std::memory_order_release)) {
          new_head &= clear_bit;  // Unset the occupied bit, signaling that
                                  // finished popping
          this->m_head->store(new_head, std::memory_order_seq_cst);
          if (value == Erased) {  // If the element was `Erased`, try to
                                  // pop again
            break;
          }
          return value;
        }
        std::this_thread::yield();
      }
    }
    std::this_thread::yield();
  }
  return Null;
}

template <typename T, T Null, T Erased, typename I, typename A>
container::Integrals_lockfree_queue<T, Null, Erased, I, A>
    &container::Integrals_lockfree_queue<T, Null, Erased, I, A>::push(
        value_type to_push) {
  assert(to_push != Null && to_push != Erased);
  this->clear_state();
  for (; true;) {
    auto tail = this->m_tail->load(std::memory_order_acquire) & clear_bit;
    auto head = this->m_head->load(std::memory_order_relaxed) & clear_bit;

    if (tail == head + this->m_capacity) {
      this->state() = enum_queue_state::NO_SPACE_AVAILABLE;
      return (*this);
    }

    auto new_tail = tail + 1;
    new_tail |= set_bit;  // Set the occupied bit
    if (this->m_tail->compare_exchange_strong(
            tail, new_tail,  // Concurrent push may have reached the CAS first
                             // or the occupied bit hasn't been unset yet
            std::memory_order_release)) {
      auto &current = this->m_array[this->translate(tail)];

      for (; true;) {
        T null_ref{Null};
        if (current.compare_exchange_strong(
                null_ref, to_push,  // It may not be `Null` if some concurrent
                                    // pop operation hasn't finished setting the
                                    // element value to `Null
                std::memory_order_acquire)) {
          new_tail &= clear_bit;  // Unset the occupied bit, signaling that
                                  // finished pushing
          this->m_tail->store(new_tail, std::memory_order_seq_cst);
          break;
        }
        std::this_thread::yield();
      }
      break;
    }
    std::this_thread::yield();
  }
  return (*this);
}

template <typename T, T Null, T Erased, typename I, typename A>
typename container::Integrals_lockfree_queue<T, Null, Erased, I, A>::Iterator
container::Integrals_lockfree_queue<T, Null, Erased, I, A>::begin() const {
  return Iterator{*this, this->head()};
}

template <typename T, T Null, T Erased, typename I, typename A>
typename container::Integrals_lockfree_queue<T, Null, Erased, I, A>::Iterator
container::Integrals_lockfree_queue<T, Null, Erased, I, A>::end() const {
  return Iterator{*this, this->tail()};
}

template <typename T, T Null, T Erased, typename I, typename A>
template <typename D, T M, T F, typename J, typename B, typename Pred,
          std::enable_if_t<M != F> *>
size_t container::Integrals_lockfree_queue<T, Null, Erased, I, A>::erase_if(
    Pred predicate) {
  this->clear_state();
  size_t erased{0};
  for (size_t idx = 0; idx != this->m_capacity; ++idx) {
    auto &current = this->m_array[idx];
    T value = current.load(std::memory_order_acquire);
    if (value != Null && value != Erased && predicate(value)) {
      if (current.compare_exchange_strong(value, Erased,
                                          std::memory_order_release)) {
        ++erased;
      }
    }
  }
  return erased;
}

template <typename T, T Null, T Erased, typename I, typename A>
size_t container::Integrals_lockfree_queue<T, Null, Erased, I, A>::capacity()
    const {
  return this->m_capacity;
}

template <typename T, T Null, T Erased, typename I, typename A>
size_t container::Integrals_lockfree_queue<T, Null, Erased, I,
                                           A>::allocated_size() const {
  return this->m_array.allocated_size();
}

template <typename T, T Null, T Erased, typename I, typename A>
container::Integrals_lockfree_queue<T, Null, Erased, I, A>
    &container::Integrals_lockfree_queue<T, Null, Erased, I, A>::clear_state() {
  this->state() = enum_queue_state::SUCCESS;
  return (*this);
}

template <typename T, T Null, T Erased, typename I, typename A>
typename container::Integrals_lockfree_queue<T, Null, Erased, I,
                                             A>::enum_queue_state
container::Integrals_lockfree_queue<T, Null, Erased, I, A>::get_state() const {
  return this->state();
}

template <typename T, T Null, T Erased, typename I, typename A>
std::string
container::Integrals_lockfree_queue<T, Null, Erased, I, A>::to_string() const {
  std::ostringstream out;
  for (auto value : (*this)) {
    out << (value == Null
                ? "Null"
                : (value == Erased ? "Erased" : std::to_string(value)))
        << ", ";
  }
  out << "EOF" << std::flush;
  return out.str();
}

template <typename T, T Null, T Erased, typename I, typename A>
size_t container::Integrals_lockfree_queue<T, Null, Erased, I, A>::translate(
    index_type from) const {
  return static_cast<size_t>(from % static_cast<index_type>(this->m_capacity));
}

template <typename T, T Null, T Erased, typename I, typename A>
typename container::Integrals_lockfree_queue<T, Null, Erased, I,
                                             A>::enum_queue_state &
container::Integrals_lockfree_queue<T, Null, Erased, I, A>::state() const {
  // TODO: garbage collect this if queues start to be used more dynamically
  static thread_local std::map<
      container::Integrals_lockfree_queue<T, Null, Erased, I, A> const *,
      enum_queue_state>
      state;
  return state[this];
}

#endif  // CONTAINER_INTEGRALS_LOCKFREE_QUEUE_INCLUDED
