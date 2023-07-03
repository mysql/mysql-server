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

#ifndef CONTAINER_ARRAY_ATOMICS_INDEX_INTERLEAVED
#define CONTAINER_ARRAY_ATOMICS_INDEX_INTERLEAVED

#include <algorithm>
#include <atomic>
#include <cmath>
#include <map>
#include <memory>
#include <sstream>
#include <thread>
#include <tuple>

#include "sql/memory/aligned_atomic.h"  // memory::minimum_cacheline_for

namespace container {

/**
  Indexing provider that interleaves sequentially stored array elements in
  order to keep them from being pulled to the same cache line, in order to
  avoid false sharing and cache misses.

  However, false sharing is only avoided for particular access patterns,
  namely, when it is heuristically unlikely (or impossible) that concurrent
  threads access array elements that are far apart.

  The array layout is as follows. When each cache line has capacity for C
  array elements, the array is sliced into C sub-arrays. The sub-arrays are
  stored in an interleaved manner, such that the i'th sub-array uses the
  i'th element within each cache line. For instance, if the machine uses
  128 byte cache lines, and an array has 6 elements, and each elements uses
  64 bytes, the array will be laid out as follows:

  | byte position | element number | cache line # |
  | 0             | 0              | 0            |
  | 64            | 3              | 0            |
  | 128           | 1              | 1            |
  | 192           | 4              | 1            |
  | 256           | 2              | 2            |
  | 320           | 5              | 2            |

  This class translates element-to-byte indexing as if each consecutive
  array index has CPU cache line bytes between them, hence, interleaving
  consecutive array indexes. The CPU cache line size is calculated at
  runtime.
 */
template <typename T>
class Interleaved_indexing {
 public:
  using type = std::atomic<T>;

  /**
    Constructor for the class that takes the number of elements in the
    array. This value may be increased in order to address CPU cache line
    alignment.
   */
  Interleaved_indexing(size_t max_size);
  /**
    Class destructor.
   */
  virtual ~Interleaved_indexing() = default;
  /**
    The size of the array.

    @return the size of the array
   */
  size_t size() const;
  /**
    Translates the element index to the byte position in the array.

    @param index the element index to be translated.

    @return the byte position in the byte array.
   */
  size_t translate(size_t index) const;
  /**
    The array element size, in bytes.

    @return The array element size, in bytes.
   */
  static size_t element_size();

 private:
  /** The array size */
  size_t m_size{0};
  /** The size of the CPU cache line */
  size_t m_cacheline_size{0};
  /** The number of array elements that fit a cache line */
  size_t m_page_size{0};
  /** The number of cache lines that fit in the byte array */
  size_t m_pages{0};
};
}  // namespace container

template <typename T>
container::Interleaved_indexing<T>::Interleaved_indexing(size_t size)
    : m_cacheline_size{memory::minimum_cacheline_for<
          Interleaved_indexing::type>()},
      m_page_size{m_cacheline_size / sizeof(Interleaved_indexing::type)},
      m_pages{std::max(static_cast<size_t>(1),
                       (size + m_page_size - 1) / m_page_size)} {
  this->m_size = this->m_pages * this->m_page_size;
}

template <typename T>
size_t container::Interleaved_indexing<T>::size() const {
  return this->m_size;
}

template <typename T>
size_t container::Interleaved_indexing<T>::translate(size_t index) const {
  return (((index % this->m_pages) * this->m_page_size) +
          (index / this->m_pages)) *
         sizeof(Interleaved_indexing::type);
}

template <typename T>
size_t container::Interleaved_indexing<T>::element_size() {
  return sizeof(Interleaved_indexing::type);
}

#endif  // CONTAINER_ARRAY_ATOMICS_INDEX_INTERLEAVED
