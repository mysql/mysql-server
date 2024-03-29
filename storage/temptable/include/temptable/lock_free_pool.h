/* Copyright (c) 2020, 2023, Oracle and/or its affiliates.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2.0, as published by the
Free Software Foundation.

This program is also distributed with certain software (including but not
limited to OpenSSL) that is licensed under separate terms, as designated in a
particular file or component or in included license documentation. The authors
of MySQL hereby grant you an additional permission to link the program and
your derivative works with the separately licensed software that they have
included with MySQL.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License, version 2.0,
for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/** @file storage/temptable/include/temptable/lock_free_pool.h
Lock-free pool implementation. */

#ifndef TEMPTABLE_LOCK_FREE_POOL_H
#define TEMPTABLE_LOCK_FREE_POOL_H

#include <array>

#include "storage/temptable/include/temptable/lock_free_type.h"

namespace temptable {

/** Lock-free pool which consists of POOL_SIZE Lock_free_type elements. It has
 * all the guarantees and properties of Lock_free_type and its consisting pieces
 * so for more details please consult the documentation of those. E.g. user code
 * can opt-in for different alignment-requirements and/or lock-free type
 * selection algorithms. This type is envisioned to be used as a building block
 * for higher-abstraction (lock-free) ADTs.
 * */
template <typename T, size_t POOL_SIZE,
          Alignment ALIGNMENT = Alignment::NATURAL,
          template <typename, typename = void> class TypeSelector =
              Lock_free_type_selector>
class Lock_free_pool {
  std::array<Lock_free_type<T, ALIGNMENT, TypeSelector>, POOL_SIZE> m_lock_free;

 public:
  using Type = typename Lock_free_type<T, ALIGNMENT, TypeSelector>::Type;

  /** Default constructor. Uses value-initialization to initialize underlying
   * T's.*/
  Lock_free_pool() : m_lock_free() {}

  /** Constructor. Uses explicit value to initialize underlying T's. */
  Lock_free_pool(Lock_free_pool::Type default_value) {
    for (auto &v : m_lock_free) v.m_value = default_value;
  }

  /** Copy-constructor and copy-assignment operator are disabled. */
  Lock_free_pool(const Lock_free_pool &) = delete;
  Lock_free_pool &operator=(const Lock_free_pool &) = delete;

  /** Ditto for move-constructor and move-assignment operator. */
  Lock_free_pool(Lock_free_pool &&) = delete;
  Lock_free_pool &operator=(Lock_free_pool &&) = delete;

  /** Atomically replaces current value in an array at given index.
   *
   * [in] idx Index of an element whose value is to be replaced.
   * [in] value Value to store into the atomic variable.
   * [in] order Memory order constraints to enforce.
   * */
  void store(size_t idx, Lock_free_pool::Type value,
             std::memory_order order = std::memory_order_seq_cst) {
    m_lock_free[idx].m_value.store(value, order);
  }

  /** Atomically loads and returns the current value from an array at given
   * index.
   *
   * [in] idx Index of an element whose value is to be returned.
   * [in] order Memory order constraints to enforce.
   * @return Returns the current value from given index.
   * */
  Lock_free_pool::Type load(
      size_t idx, std::memory_order order = std::memory_order_seq_cst) const {
    return m_lock_free[idx].m_value.load(order);
  }

  /** Atomically compares the object representation of an array element at given
   *  index with the expected, and if those are bitwise-equal, replaces the
   * former with desired.
   *
   * [in] idx Index of an element whose value is to be compared against.
   * [in] expected Value expected to be found in the atomic object. Gets stored
   * with the actual value of *this if the comparison fails.
   * [in] desired Value to store in the atomic object if it is as expected.
   * [in] order Memory order constraints to enforce.
   * @return True if the underlying atomic value was successfully changed, false
   * otherwise.
   * */
  bool compare_exchange_strong(
      size_t idx, Lock_free_pool::Type &expected, Lock_free_pool::Type desired,
      std::memory_order order = std::memory_order_seq_cst) {
    return m_lock_free[idx].m_value.compare_exchange_strong(expected, desired,
                                                            order);
  }

  /** Returns the number of elements this pool contains.
   *
   * @return Number of elements in the pool.
   * */
  constexpr size_t size() const { return POOL_SIZE; }
};

}  // namespace temptable

#endif /* TEMPTABLE_LOCK_FREE_POOL_H */
