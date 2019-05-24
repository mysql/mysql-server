#ifndef SQL_INTRUSIVE_LIST_ITERATOR_H_
#define SQL_INTRUSIVE_LIST_ITERATOR_H_
/* Copyright (c) 2019, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

/**
  @file intrusive_list_iterator.h

  Iterator utilities for working with intrusive pointers.
*/

#include "my_dbug.h"

/**
  An iterator that follows a 'next' pointer with an accessor function.
  @tparam T The type of the object holding the intrusive list.
  @tparam GetNextPointer The accessor function, returning a pointer to the
  next object in the list.

  @note Due to the nature of intrusive 'next' pointers it's not possible to
  free an intrusive pointee while iterating over an intrusive list with
  the pre-increment operator, as the enhanced for-loop does, e.g.

  ```
  for(auto elem : elems)
    delete *elem;
  ```

  Will cause a core dump. However, the following is possible:

  ```
  auto it = container.begin();
  while(it != container.end()) delete *(it++);
  ```
*/
template <typename T, T *(*GetNextPointer)(const T *)>
class NextPointerIterator {
 public:
  using value_type = T *;
  /**
    Constructs an iterator.

    @param start The object that the iterator will start iterating
    from.
  */
  explicit NextPointerIterator(T *start) : m_current(start) {}

  /// Constructs a past-the-end iterator.
  NextPointerIterator() : m_current(nullptr) {}

  NextPointerIterator &operator++() {
    DBUG_ASSERT(m_current != nullptr);
    m_current = GetNextPointer(m_current);
    return *this;
  }

  NextPointerIterator operator++(int) {
    auto pre_increment(*this);
    ++(*this);
    return pre_increment;
  }

  T *operator*() const { return m_current; }

  bool operator==(const NextPointerIterator &other) const {
    return m_current == other.m_current;
  }

  bool operator!=(const NextPointerIterator &other) const {
    return !((*this) == other);
  }

 private:
  T *m_current;
};

/**
  Helper template for the case when the 'next' member can be used directly,
  typically when it's public and the class definition is known.
*/
template <typename T, T *T::*Member>
T *GetMember(const T *t) {
  return t->*Member;
}

/**
  An iterator that follows the 'next' pointer in an intrusive list.
  Conforms to the ForwardIterator named requirement.

  @tparam T The type of the object holding the intrusive list.
  @tparam NextPointer The intrusive list's "next" pointer member.
*/
template <typename T, T *T::*NextPointer>
class IntrusiveListIterator
    : public NextPointerIterator<T, GetMember<T, NextPointer>> {
 public:
  IntrusiveListIterator() = default;
  explicit IntrusiveListIterator(T *t)
      : NextPointerIterator<T, GetMember<T, NextPointer>>(t) {}
};

/**
  Adds a collection interface on top of an iterator. The iterator must support a
  default constructor constructing a past-the-end iterator.

  @tparam IteratorType The iterator's class.
*/
template <typename IteratorType>
class IteratorContainer {
 public:
  using Type = typename IteratorType::value_type;
  explicit IteratorContainer(Type first) : m_first(first) {}

  IteratorType begin() { return IteratorType(m_first); }
  IteratorType end() { return IteratorType(); }

 private:
  Type m_first;
};

/**
  Convenience alias for instantiating a container directly from the accessor
  function.
*/
template <typename T, T *(*GetNextPointer)(const T *)>
using NextFunctionContainer =
    IteratorContainer<NextPointerIterator<T, GetNextPointer>>;

template <typename T, T *T::*NextPointer>
using NextPointerContainer =
    NextFunctionContainer<T, GetMember<T, NextPointer>>;

#endif  // SQL_INTRUSIVE_LIST_ITERATOR_H_
