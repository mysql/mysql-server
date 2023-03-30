/* Copyright (c) 2020, 2023, Oracle and/or its affiliates.

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

#ifndef CONTAINER_ARRAY_ATOMICS_INDEX_PADDING
#define CONTAINER_ARRAY_ATOMICS_INDEX_PADDING

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
  Indexing provider that pads each of the array elements to the size of the
  CPU cache line, in order to avoid false sharing and cache misses.

  In terms of the array size, this indexing provider will use force the use
  of more memory than the needed to store the array elements of type
  `T`. If the array is of size `n`, then instead of the allocated memory
  being `n * sizeof(std::atomic<T>)`, it will be of size `n *
  cache_line_size`. Since, typically, in modern systems, the cache line
  size is 64 or 128 bytes, that would be an increase in the allocated
  memory.

  This class translates element-to-byte indexing as if each element is
  aligned with the size of the CPU cache line. The CPU cache line size is
  calculated at runtime.
 */
template <typename T>
class Padded_indexing {
 public:
  using type = std::atomic<T>;

  /**
    Constructor for the class that takes the maximum allowed number of
    elements in the array.
   */
  Padded_indexing(size_t max_size);
  /**
    Class destructor.
   */
  virtual ~Padded_indexing() = default;
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
  /** The size of the array */
  size_t m_size{0};
  /** The size of the CPU cache line */
  size_t m_cacheline_size{0};
};
}  // namespace container

template <typename T>
container::Padded_indexing<T>::Padded_indexing(size_t size)
    : m_size{size},
      m_cacheline_size{memory::minimum_cacheline_for<Padded_indexing::type>()} {
}

template <typename T>
size_t container::Padded_indexing<T>::size() const {
  return this->m_size;
}

template <typename T>
size_t container::Padded_indexing<T>::translate(size_t index) const {
  return index * this->m_cacheline_size;
}

template <typename T>
size_t container::Padded_indexing<T>::element_size() {
  return memory::minimum_cacheline_for<Padded_indexing::type>();
}

#endif  // CONTAINER_ARRAY_ATOMICS_INDEX_PADDING
