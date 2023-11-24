#ifndef VARLEN_SORT_INCLUDED
#define VARLEN_SORT_INCLUDED

/* Copyright (c) 2017, 2023, Oracle and/or its affiliates.

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

#include <assert.h>

#include "template_utils.h"

#include <algorithm>
#include <cstddef>
#include <iterator>

template <class RandomIt, class Compare>
void my_insert_sort(RandomIt first, RandomIt last, Compare comp) {
  for (RandomIt high_water_mark = first + 1; high_water_mark < last;
       ++high_water_mark) {
    for (RandomIt cur = high_water_mark; cur > first; --cur) {
      RandomIt prev = cur - 1;
      if (comp(*prev, *cur)) break;
      std::iter_swap(cur - 1, cur);
    }
  }
}

/*
  Conceptually similar to a struct { uchar[N] },
  except that it just holds a reference to an underlying
  array, instead of keeping the memory itself.
*/
struct varlen_element {
  varlen_element(unsigned char *ptr_arg, size_t elem_size_arg)
      : ptr(ptr_arg), elem_size(elem_size_arg) {}

  varlen_element(const varlen_element &other) = delete;
  varlen_element(varlen_element &&other) = delete;
  varlen_element &operator=(const varlen_element &other) = delete;
  varlen_element &operator=(varlen_element &&other) = delete;

  unsigned char *ptr{nullptr};
  size_t elem_size{0};
};

// ValueSwappable.
inline void swap(const varlen_element &a, const varlen_element &b) {
  assert(a.elem_size == b.elem_size);
  std::swap_ranges(a.ptr, a.ptr + a.elem_size, b.ptr);
}

/**
  A RandomAccessIterator that splits up a char/uchar array into fixed-length
  elements. Conceptually similar to a _pointer_ to an uchar[N].
*/
class varlen_iterator {
 public:
  varlen_iterator(unsigned char *ptr_arg, size_t elem_size_arg)
      : ptr(ptr_arg), elem_size(elem_size_arg) {}

  // Iterator (required for InputIterator).
  varlen_element operator*() const { return varlen_element{ptr, elem_size}; }
  varlen_iterator &operator++() {
    ptr += elem_size;
    return *this;
  }

  // EqualityComparable (required for InputIterator).
  bool operator==(const varlen_iterator &other) const {
    assert(elem_size == other.elem_size);
    return ptr == other.ptr;
  }

  // InputIterator (required for ForwardIterator).
  bool operator!=(const varlen_iterator &other) const {
    return !(*this == other);
  }

  varlen_element operator->() const { return varlen_element{ptr, elem_size}; }

  // DefaultConstructible (required for ForwardIterator).
  varlen_iterator() = default;

  // ForwardIterator (required for RandomAccessIterator).
  varlen_iterator operator++(int) {
    varlen_iterator copy = *this;
    ptr += elem_size;
    return copy;
  }

  // BidirectionalIterator (required for RandomAccessIterator).
  varlen_iterator &operator--() {
    ptr -= elem_size;
    return *this;
  }
  varlen_iterator operator--(int) {
    varlen_iterator copy = *this;
    ptr -= elem_size;
    return copy;
  }

  // RandomAccessIterator.
  varlen_iterator &operator+=(size_t n) {
    ptr += elem_size * n;
    return *this;
  }

  varlen_iterator &operator-=(size_t n) {
    ptr -= elem_size * n;
    return *this;
  }

  varlen_iterator operator+(size_t n) const {
    return varlen_iterator{ptr + elem_size * n, elem_size};
  }

  varlen_iterator operator-(size_t n) const {
    return varlen_iterator{ptr - elem_size * n, elem_size};
  }

  ptrdiff_t operator-(const varlen_iterator &other) const {
    assert(elem_size == other.elem_size);
    assert((ptr - other.ptr) % elem_size == 0);
    return (ptr - other.ptr) / elem_size;
  }

  varlen_element operator[](size_t i) const {
    return varlen_element{ptr + i * elem_size, elem_size};
  }

  bool operator<(varlen_iterator &other) const {
    assert(elem_size == other.elem_size);
    return ptr < other.ptr;
  }

  bool operator>(varlen_iterator &other) const {
    assert(elem_size == other.elem_size);
    return ptr > other.ptr;
  }

  bool operator>=(varlen_iterator &other) const {
    assert(elem_size == other.elem_size);
    return ptr >= other.ptr;
  }

  bool operator<=(varlen_iterator &other) const {
    assert(elem_size == other.elem_size);
    return ptr <= other.ptr;
  }

 private:
  unsigned char *ptr = nullptr;
  size_t elem_size = 0;
};

namespace std {

// Required for Iterator.
template <>
struct iterator_traits<varlen_iterator> : iterator_traits<varlen_element *> {
  using reference = varlen_element;
};
}  // namespace std

/*
  Compare should be a functor that takes in two T*.
  T does not need to be char or uchar.
*/
template <class T, class Compare>
inline void varlen_sort(T *first, T *last, size_t elem_size, Compare comp) {
  my_insert_sort(
      varlen_iterator(pointer_cast<unsigned char *>(first), elem_size),
      varlen_iterator(pointer_cast<unsigned char *>(last), elem_size),
      [comp](const varlen_element &a, const varlen_element &b) {
        return comp(pointer_cast<T *>(a.ptr), pointer_cast<T *>(b.ptr));
      });
}

#endif  // !defined(VARLEN_SORT_INCLUDED)
