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

/** @file storage/temptable/include/temptable/sharded_kv_store_logger.h
TempTable sharded key-value store logger implementation. */

#ifndef TEMPTABLE_SHARDED_KV_STORE_LOGGER_H
#define TEMPTABLE_SHARDED_KV_STORE_LOGGER_H

#include <algorithm>
#include <string>
#include <thread>

#include "my_dbug.h"
#include "storage/temptable/include/temptable/kv_store_stats.h"

namespace temptable {

/** Default Sharded_key_value_store logging facility which turns to no-op in
 * non-debug builds. */
template <typename T, bool DebugBuild>
struct Sharded_key_value_store_logger {
  void dbug_print() {
    // no-op
  }
};

/** Sharded_key_value_store logging facility debug builds only. */
template <typename T>
struct Sharded_key_value_store_logger<T, true> {
  /** DBUG_PRINT's the stats of each shard and additionally some further stats
   * which are deduced from it, e.g. ratio between insertions and removals.
   * */
  void dbug_print() {
    // As representation of std::thread::id is implementation defined, we have
    // to convert it to a string first before passing it over to DBUG_PRINT
    // machinery. Conversion to string can be done with the overloaded
    // std::thread::id::operator<< which is part of the library
    // implementation.
    auto std_thread_id_to_str = [](const std::thread::id &id) {
      std::stringstream ss;
      ss << id;
      return ss.str();
    };

    auto &kv_store_shards = static_cast<T &>(*this).m_kv_store_shard;
    uint32_t shard_id [[maybe_unused]] = 0;
    for (auto &kv : kv_store_shards) {
      auto kv_shard_stats = kv.shard.stats();
      size_t nr_of_emplace_events = std::count_if(
          kv_shard_stats.begin(), kv_shard_stats.end(), [](auto &stat) {
            return stat.event == Key_value_store_stats::Event::EMPLACE;
          });

      // Silence the -Wunused-variable false-positive warning (bug) from clang.
      // MY_COMPILER_CLANG_WORKAROUND_FALSE_POSITIVE_UNUSED_VARIABLE_WARNING
      // documentation contains more details (e.g. specific bug-number from LLVM
      // Bugzilla)
      MY_COMPILER_DIAGNOSTIC_PUSH()
      MY_COMPILER_CLANG_WORKAROUND_FALSE_POSITIVE_UNUSED_VARIABLE_WARNING()
      size_t nr_of_erase_events = kv_shard_stats.size() - nr_of_emplace_events;
      MY_COMPILER_DIAGNOSTIC_POP()

      DBUG_PRINT("temptable_api_sharded_kv_store",
                 ("shard_id=%u insertions=%zu removals=%zu", shard_id,
                  nr_of_emplace_events, nr_of_erase_events));
      for (auto &stat : kv_shard_stats) {
        DBUG_PRINT(
            "temptable_api_sharded_kv_store_debug",
            ("shard_id=%u event=%d size=%zu bucket_count=%zu load_factor=%f "
             "max_load_factor=%f "
             "max_bucket_count=%zu thread_id=%s",
             shard_id, static_cast<int>(stat.event), stat.size,
             stat.bucket_count, stat.load_factor, stat.max_load_factor,
             stat.max_bucket_count,
             std_thread_id_to_str(stat.thread_id).c_str()));
      }
      shard_id++;
    }
  }
};

}  // namespace temptable

#endif /* TEMPTABLE_SHARDED_KV_STORE_LOGGER_H */
