/* Copyright (c) 2020, 2022, Oracle and/or its affiliates.

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

/** @file storage/temptable/include/temptable/kv_store.h
TempTable key-value store implementation. */

#ifndef TEMPTABLE_KV_STORE_H
#define TEMPTABLE_KV_STORE_H

#include <shared_mutex>
#include <string>
#include <unordered_map>

#include "storage/temptable/include/temptable/constants.h"
#include "storage/temptable/include/temptable/kv_store_logger.h"

namespace temptable {

/** Forward-declarations. */
class Table;

/** Key-value store, a convenience wrapper class which models a thread-safe
 * dictionary type.
 *
 * Thread-safety is accomplished by using a `Lock` which can be any of the usual
 * mutual exclusive algorithms from C++ thread-support library or any other
 * which satisfy C++ concurrency named requirements. E.g. mutex, timed_mutex,
 * recursive_mutex, recursive_timed_mutex, shared_mutex, shared_timed_mutex, ...
 * User code can opt-in for any of those. Also, whether the actual
 * implementation will use shared-locks or exclusive-locks (for read-only
 * operations) is handled automagically by this class.
 *
 * Furthermore, user code can similarly opt-in for different key-value
 * implementation but whose interface is compatible with the one of
 * std::unordered_map.
 * */
template <typename Lock,
          template <typename...> class KeyValueImpl = std::unordered_map>
class Key_value_store
    : public Key_value_store_logger<Key_value_store<Lock, KeyValueImpl>,
                                    DEBUG_BUILD> {
  /** Do not break encapsulation when using CRTP. */
  friend class Key_value_store_logger<Key_value_store<Lock, KeyValueImpl>,
                                      DEBUG_BUILD>;

  /** Help ADL to bring debugging/logging symbols into the scope. */
  using Key_value_store_logger<Key_value_store<Lock, KeyValueImpl>,
                               DEBUG_BUILD>::dbug_print;
  using Key_value_store_logger<Key_value_store<Lock, KeyValueImpl>,
                               DEBUG_BUILD>::log;

  /** Check whether we can use shared locks (which enable multiple concurrent
   * readers) or must we rather fallback to exclusive locks. Shared-locks will
   * be used only during read-only operations.
   * */
  using Exclusive_or_shared_lock =
      std::conditional_t<std::is_same<Lock, std::shared_timed_mutex>::value,
                         std::shared_lock<Lock>, std::lock_guard<Lock>>;

  /** Alias for our key-value store implementation. */
  using Key_value_store_impl = KeyValueImpl<std::string, Table>;

  /** Container holding (table-name, Table) tuples. */
  Key_value_store_impl m_kv_store;

  /** Lock type. */
  Lock m_lock;

 public:
  /** Inserts a new table into the container constructed in-place with the
   * given args if there is no table with the key in the container.
   *
   * [in] args Arguments to forward to the constructor of the table.
   * @return Returns a pair consisting of an iterator to the inserted table,
   * or the already-existing table if no insertion happened, and a bool
   * denoting whether the insertion took place (true if insertion happened,
   * false if it did not).
   * */
  template <class... Args>
  std::pair<typename Key_value_store_impl::iterator, bool> emplace(
      Args &&... args) {
    std::lock_guard<Lock> lock(m_lock);
    dbug_print();
    log(Key_value_store_stats::Event::EMPLACE);
    return m_kv_store.emplace(args...);
  }

  /** Searches for a table with given name (key).
   *
   * [in] key Name of a table to search for.
   * @return Pointer to table if found, nullptr otherwise.
   * */
  Table *find(const std::string &key) {
    Exclusive_or_shared_lock lock(m_lock);
    auto iter = m_kv_store.find(key);
    if (iter != m_kv_store.end()) {
      return &iter->second;
    } else {
      return nullptr;
    }
  }

  /** Removes the table (if one exists) with the given name (key).
   *
   * [in] key Name of a table to remove..
   * @return Number of elements removed.
   * */
  typename Key_value_store_impl::size_type erase(const std::string &key) {
    std::lock_guard<Lock> lock(m_lock);
    dbug_print();
    log(Key_value_store_stats::Event::ERASE);
    return m_kv_store.erase(key);
  }
};

}  // namespace temptable

#endif /* TEMPTABLE_KV_STORE_H */
