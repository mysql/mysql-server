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

/** @file storage/temptable/include/temptable/kv_store_logger.h
TempTable key-value store logger implementation. */

#ifndef TEMPTABLE_KV_STORE_LOGGER_H
#define TEMPTABLE_KV_STORE_LOGGER_H

#include <thread>
#include <vector>

#include "my_dbug.h"
#include "storage/temptable/include/temptable/kv_store_stats.h"

namespace temptable {

/** Default Key_value_store logging facility which turns to no-op in non-debug
 * builds.
 */
template <typename T, bool DebugBuild>
class Key_value_store_logger {
 protected:
  void log(Key_value_store_stats::Event /*event*/) {
    // no-op
  }
  void dbug_print() {
    // no-op
  }
};

/** Key_value_store logging facility debug builds only. */
template <typename T>
class Key_value_store_logger<T, true> {
  /** Container of stats that we will be collecting. */
  std::vector<Key_value_store_stats> m_stats;

 public:
  /** Returns a snapshot of stats collected. To keep up with the thread-safety
   * guarantee promise, snapshot must be made under the lock.
   *
   * @return Stats snapshot.
   * */
  std::vector<Key_value_store_stats> stats() {
    auto &kv_store_lock = static_cast<T &>(*this).m_lock;
    typename T::Exclusive_or_shared_lock lock(kv_store_lock);
    return m_stats;
  }

 protected:
  /** Appends a new entry to stats container with the given event.
   *
   * [in] event Type of event to be logged.
   * */
  void log(Key_value_store_stats::Event event) {
    const auto &kv_store = static_cast<T &>(*this).m_kv_store;
    m_stats.push_back({event, kv_store.size(), kv_store.bucket_count(),
                       kv_store.load_factor(), kv_store.max_load_factor(),
                       kv_store.max_bucket_count(),
                       std::this_thread::get_id()});
  }

  /** DBUG_PRINT's the stats of underlying key-value store implementation.
   * */
  void dbug_print() {
    const auto &kv_store = static_cast<T &>(*this).m_kv_store;
    DBUG_PRINT(
        "temptable_api_kv_store",
        ("this=%p size=%zu; bucket_count=%zu load_factor=%f "
         "max_load_factor=%f "
         "max_bucket_count=%zu",
         this, kv_store.size(), kv_store.bucket_count(), kv_store.load_factor(),
         kv_store.max_load_factor(), kv_store.max_bucket_count()));
  }
};

}  // namespace temptable

#endif /* TEMPTABLE_KV_STORE_LOGGER_H */
