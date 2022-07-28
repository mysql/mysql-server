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

#ifndef CONTAINER_ATOMIC_INTEGRALS_ARRAY
#define CONTAINER_ATOMIC_INTEGRALS_ARRAY

#include <algorithm>
#include <atomic>
#include <cmath>
#include <map>
#include <memory>
#include <sstream>
#include <thread>
#include <tuple>

#include "sql/containers/atomics_array_index_padding.h"  // container::Padded_Indexing
#include "sql/memory/unique_ptr.h"                       // memory::Unique_ptr

namespace container {
/**
  Array of `std::atomic` elements of type `T`.

  An array of `std::atomic` elements implies, almost for sure, a
  multi-threaded environment and concurrent access to the array may lead to
  false sharing when consecutive elements are pulled into the same CPU
  cache line. This class accepts, as a template parameter, a helper class
  that is both an element storage index translator and element storage size
  provider. Different strategies to prevent false sharing and the
  subsequent cache invalidation and misses, may be applied. Amongst others,
  padding each element of the array to the size of the cache line or use
  index translation to interleave sequential indexes to leave enough space
  for them not to be pulled into the same cache line. The two described
  strategies are provided already with the `container::Padded_indexing` and
  `container::Interleaved_indexing` classes.

  Template parameters are as follows:
  - `T`: the integral type for the array elements.
  - `I`: type of indexing to be used by this array in the form of a
         class. Available classes are `container::Padded_indexing` and
         `container::Interleaved_indexing`. The parameter defaults to
         `container::Padded_indexing`.
  - `A`: type of memory allocator to be used, in the form of a class
         (defaults to no allocator).

  When and if deciding between interleaved or padded indexing, one could
  take into consideration, amongst possibly others, the following
  arguments:

  - For arrays with random concurrent access patterns, interleaved indexing
    doesn't ensure false-sharing prevention.
  - For arrays with sequential concurrent access patterns, if it's needed
    that interleaved indexing prevents false-sharing, consecutive array
    indexes will need to be apart, physically, the size of a
    cache-line. So, in a system with expectation of T threads accessing
    concurrently an array of elements of size `E` and with a cache-line of
    size `CL`, the array capacity should be at least `T * (CL / E)` for
    interleaved indexing to prevent false-sharing.
  - Padded indexing will always prevent false-sharing but it will consume
    more memory in order to have the same array capacity as the interleaved
    indexing.
 */
template <typename T, typename I = container::Padded_indexing<T>,
          typename A = std::nullptr_t>
class Atomics_array {
 public:
  using pointer_type = T *;
  using const_pointer_type = T const *;
  using reference_type = T &;
  using const_reference_type = T const &;
  using value_type = T;
  using const_value_type = T const;
  using element_type = std::atomic<T>;

  /**
    Iterator helper class to iterate over the array, from 0 to the array
    size.

    Check C++ documentation on `Iterator named requirements` for more
    information on the implementation.
   */
  class Iterator {
   public:
    using difference_type = std::ptrdiff_t;
    using value_type = T;
    using pointer = T *;
    using reference = T;
    using iterator_category = std::forward_iterator_tag;

    /**
     */
    explicit Iterator(Atomics_array<T, I, A> const &parent, size_t position);
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

   private:
    /** The position of the element this iterator is pointing to. */
    size_t m_current{0};
    /** The reference to the queue holding the elements. */
    Atomics_array<T, I, A> const *m_parent{nullptr};
  };

  /**
    Constructor allowing a specific memory allocator, a specific queue size
    and the instance of `T` to initialize the array with.

    @param alloc The memory allocator instance
    @param size The desired size for the queue
    @param init_value The instance of `T` to initialize the array with
   */
  template <
      typename D = T, typename J = I, typename B = A,
      std::enable_if_t<!std::is_same<B, std::nullptr_t>::value> * = nullptr>
  Atomics_array(A &alloc, size_t size, T init_value);
  /**
    Constructor allowing a specific queue size and the instance of `T` to
    initialize the array with.

    @param size The desired size for the queue
    @param init_value The instance of `T` to initialize the array with
   */
  template <
      typename D = T, typename J = I, typename B = A,
      std::enable_if_t<std::is_same<B, std::nullptr_t>::value> * = nullptr>
  Atomics_array(size_t size, T init_value);
  // Deleted copy and move constructors.
  Atomics_array(Atomics_array<T, I, A> const &rhs) = delete;
  Atomics_array(Atomics_array<T, I, A> &&rhs) = delete;
  //
  /**
    Destructor for the class.
   */
  virtual ~Atomics_array() = default;
  // Deleted copy and move operators.
  Atomics_array<T, I, A> &operator=(Atomics_array<T, I, A> const &rhs) = delete;
  Atomics_array<T, I, A> &operator=(Atomics_array<T, I, A> &&rhs) = delete;
  //
  /**
    Retrieves the value stored in a specific index of the array.

    @param index The index of the element to retrieve the value for

    @return the value of the element at index `index`
   */
  element_type &operator[](size_t index) const;
  /**
    Retrieves an iterator instance that points to the beginning of the
    array.

    @return An instance of an iterator pointing to the beginning of the
            array.
   */
  Iterator begin() const;
  /**
    Retrieves an iterator instance that points to the end of the underlying
    array.

    @return An instance of an iterator pointing to the end of the underlying
            array.
   */
  Iterator end() const;
  /**
    Finds a value in the queue according to the evaluation of `predicate`
    by traversing the array from `start_from` to the array size.

    The find condition is given according to the predicate `predicate` which
    should be any predicate which translatable to `[](value_type value,
    size_t index) -> bool`. If the predicate evaluates to `true`, the value
    and respective index are returned as an `std::tuple`.

    Check C++ documentation for the definition of `Predicate` named requirement
    for more information.

    @param predicate The predicate to be evaluated
    @param start_from The index to start the search at

    @return A tuple holding the value and the index of the first element
            for which the predicate evaluated to true. If the predicate
            doesn't evaluate to true for any of the elements, it returns a
            pair holding the default value for `T` and the size of the
            array.

   */
  template <typename Pred>
  std::tuple<T, size_t> find_if(Pred predicate, size_t start_from = 0) const;
  /**
    First the first occurrence of `to_find`, starting at `start_from` index.

    @param to_find the value to find
    @param start_from the index to start the search from

    @return the index of the the first element that matches the `to_find`
            value or the array size if no element matches.
   */
  size_t find(T to_find, size_t start_from = 0) const;
  /**
    Returns the size of the array.

    @return The size of the array
   */
  size_t size() const;
  /**
    Returns the number of bytes used to allocate the array.

    @return The number of bytes used to allocate the array
   */
  size_t allocated_size() const;
  /**
    Return `this` queue textual representation.

    @return The textual representation for `this` queue.
   */
  std::string to_string() const;

  friend std::ostream &operator<<(std::ostream &out,
                                  Atomics_array<T, I, A> &in) {
    out << in.to_string(true) << std::flush;
    return out;
  }

 private:
  /** The index translation object to be used */
  I m_index;
  /** The byte array in which the elements will be stored */
  memory::Unique_ptr<unsigned char[], A> m_array;

  /**
    Initializes the array.

    @return The reference to `this` object, for chaining purposes.
   */
  container::Atomics_array<T, I, A> &init(T init_value);
};
}  // namespace container

template <typename T, typename I, typename A>
container::Atomics_array<T, I, A>::Iterator::Iterator(
    Atomics_array<T, I, A> const &parent, size_t position)
    : m_current{position}, m_parent{&parent} {}

template <typename T, typename I, typename A>
container::Atomics_array<T, I, A>::Iterator::Iterator(Iterator const &rhs)
    : m_current{rhs.m_current}, m_parent{rhs.m_parent} {}

template <typename T, typename I, typename A>
container::Atomics_array<T, I, A>::Iterator::Iterator(Iterator &&rhs)
    : m_current{rhs.m_current}, m_parent{rhs.m_parent} {
  rhs.m_current = 0;
  rhs.m_parent = nullptr;
}

template <typename T, typename I, typename A>
typename container::Atomics_array<T, I, A>::Iterator &
container::Atomics_array<T, I, A>::Iterator::operator=(Iterator const &rhs) {
  this->m_current = rhs.m_current;
  this->m_parent = rhs.m_parent;
  return (*this);
}

template <typename T, typename I, typename A>
typename container::Atomics_array<T, I, A>::Iterator &
container::Atomics_array<T, I, A>::Iterator::operator=(Iterator &&rhs) {
  this->m_current = rhs.m_current;
  this->m_parent = rhs.m_parent;
  rhs.m_current = 0;
  rhs.m_parent = nullptr;
  return (*this);
}

template <typename T, typename I, typename A>
typename container::Atomics_array<T, I, A>::Iterator &
container::Atomics_array<T, I, A>::Iterator::operator++() {
  if (this->m_current < this->m_parent->m_index.size()) {
    ++this->m_current;
  }
  return (*this);
}

template <typename T, typename I, typename A>
typename container::Atomics_array<T, I, A>::Iterator::reference
container::Atomics_array<T, I, A>::Iterator::operator*() const {
  return (*this->m_parent)[this->m_current];
}

template <typename T, typename I, typename A>
typename container::Atomics_array<T, I, A>::Iterator
container::Atomics_array<T, I, A>::Iterator::operator++(int) {
  auto to_return = (*this);
  ++(*this);
  return to_return;
}

template <typename T, typename I, typename A>
typename container::Atomics_array<T, I, A>::Iterator::pointer
container::Atomics_array<T, I, A>::Iterator::operator->() const {
  return &((*this->m_parent)[this->m_current]);
}

template <typename T, typename I, typename A>
bool container::Atomics_array<T, I, A>::Iterator::operator==(
    Iterator const &rhs) const {
  return this->m_current == rhs.m_current && this->m_parent == rhs.m_parent;
}

template <typename T, typename I, typename A>
bool container::Atomics_array<T, I, A>::Iterator::operator!=(
    Iterator const &rhs) const {
  return !((*this) == rhs);
}

template <typename T, typename I, typename A>
template <typename D, typename J, typename B,
          std::enable_if_t<!std::is_same<B, std::nullptr_t>::value> *>
container::Atomics_array<T, I, A>::Atomics_array(A &alloc, size_t size,
                                                 T init_value)
    : m_index{size},
      m_array{alloc, static_cast<size_t>(m_index.size()) * I::element_size()} {
  this->init(init_value);
}

template <typename T, typename I, typename A>
template <typename D, typename J, typename B,
          std::enable_if_t<std::is_same<B, std::nullptr_t>::value> *>
container::Atomics_array<T, I, A>::Atomics_array(size_t size, T init_value)
    : m_index{size},
      m_array{memory::make_unique<unsigned char[]>(
          static_cast<size_t>(m_index.size()) * I::element_size())} {
  this->init(init_value);
}

template <typename T, typename I, typename A>
typename container::Atomics_array<T, I, A>::element_type &
container::Atomics_array<T, I, A>::operator[](size_t index) const {
  return reinterpret_cast<Atomics_array::element_type &>(
      this->m_array[this->m_index.translate(index)]);
}

template <typename T, typename I, typename A>
typename container::Atomics_array<T, I, A>::Iterator
container::Atomics_array<T, I, A>::begin() const {
  return Iterator{*this, 0};
}

template <typename T, typename I, typename A>
typename container::Atomics_array<T, I, A>::Iterator
container::Atomics_array<T, I, A>::end() const {
  return Iterator{*this, this->m_index.size()};
}

template <typename T, typename I, typename A>
template <typename Pred>
std::tuple<T, size_t> container::Atomics_array<T, I, A>::find_if(
    Pred predicate, size_t start_from) const {
  for (size_t idx = start_from; idx != this->m_index.size(); ++idx) {
    auto &current = (*this)[idx];
    T value = current.load(std::memory_order_relaxed);
    if (predicate(value, idx)) {
      return std::make_tuple(value, idx);
    }
  }
  return std::make_tuple(std::numeric_limits<T>::max(), this->m_index.size());
}

template <typename T, typename I, typename A>
size_t container::Atomics_array<T, I, A>::find(T to_find,
                                               size_t start_from) const {
  for (size_t idx = start_from; idx != this->m_index.size(); ++idx) {
    auto &current = (*this)[idx];
    T value = current.load(std::memory_order_relaxed);
    if (value == to_find) {
      return idx;
    }
  }
  return this->m_index.size();
}

template <typename T, typename I, typename A>
size_t container::Atomics_array<T, I, A>::size() const {
  return this->m_index.size();
}

template <typename T, typename I, typename A>
size_t container::Atomics_array<T, I, A>::allocated_size() const {
  return this->m_index.size() * I::element_size();
}

template <typename T, typename I, typename A>
std::string container::Atomics_array<T, I, A>::to_string() const {
  std::ostringstream out;
  for (auto value : (*this)) {
    out << std::to_string(value) << ", ";
  }
  out << "EOF" << std::flush;
  return out.str();
}

template <typename T, typename I, typename A>
container::Atomics_array<T, I, A> &container::Atomics_array<T, I, A>::init(
    T init_value) {
  for (size_t idx = 0; idx != this->m_index.size(); ++idx) {
    ::new (&this->m_array[this->m_index.translate(idx)])
        element_type(init_value);
  }
  return (*this);
}

#endif  // CONTAINER_ATOMIC_INTEGRALS_ARRAY
