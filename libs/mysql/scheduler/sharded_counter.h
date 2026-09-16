// Copyright (c) 2026, Oracle and/or its affiliates.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License, version 2.0,
// as published by the Free Software Foundation.
//
// This program is designed to work with certain software (including
// but not limited to OpenSSL) that is licensed under separate terms,
// as designated in a particular file or component or in included license
// documentation.  The authors of MySQL hereby grant you an additional
// permission to link the program and your derivative works with the
// separately licensed software that they have either included with
// the program or referenced in the documentation.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License, version 2.0, for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA.

#ifndef MYSQL_SCHEDULER_SHARDED_COUNTER_H
#define MYSQL_SCHEDULER_SHARDED_COUNTER_H

#include <atomic>
#include <chrono>
#include <unordered_map>

namespace mysql::scheduler {

/// @brief Concurrent counter, relaxing contention on atomic counter when
/// being updated by different threads.
/// The caller is able to get a value for specific thread, and store the
/// value held by a specific thread (`get`, `store`).
/// In addition to that, the caller can extract
/// cumulative value for all of the threads (via `get`), atomically increment
/// the counter by a specific value, or use helper
/// time counters (`start_timer`, `stop_timer`, `get_timer`).
class Sharded_counter {
 public:
  /// @brief Obtains value for specific thread id
  /// @param thread_id Requested thread id
  long long get(std::size_t thread_id) const;
  /// @brief Coalescing get operation, available only for integer type
  long long get() const;
  /// @brief Stores value for specific thread id
  /// @param arg Update op argument
  /// @param thread_id Requested thread id
  void store(long long arg, std::size_t thread_id = 0);
  /// @brief Updates value (add operation) for specific thread id
  /// @param arg Update op argument
  /// @param thread_id Requested thread id
  void add(long long arg, std::size_t thread_id = 0);
  /// @brief Initializes this counter for requested number of threads, starting
  /// with thread id equal to 0, up to num_threads-1
  /// @param num_threads Requested number of threads
  /// @param enabled Specify whether this statistic is enabled
  void init(std::size_t num_threads, bool enabled = true);
  /// @brief Special function to handle timer (helper for add/get when builing
  /// a timer over a concurrent counter). Start timer.
  /// @param thread_id Requested thread id
  void start_time(std::size_t thread_id = 0);
  /// @brief Special function to handle timer (helper for add/get when builing
  /// a timer over a concurrent counter). Stop timer for the specified thread id
  /// @param thread_id Requested thread id
  void stop_time(std::size_t thread_id = 0);
  /// @brief Special function to handle timer (helper for add/get when builing
  /// a timer over a concurrent counter). Get timer value for the specified
  /// thread id
  /// @param thread_id Requested thread id
  /// @return Timer value for the specified thread
  long long get_timer(std::size_t thread_id) const;
  /// @brief Special function to handle timer (helper for add/get when builing
  /// a timer over a concurrent counter). Get coalesced timer value.
  /// @return Coalesced timer value
  long long get_timer() const;
  /// @brief Sets the internal value to 0
  void reset();

 protected:
  using Map_value_type = std::atomic<long long>;
  /// @brief Keeps mapping between thread id and value of the counter
  /// for this thread id
  std::unordered_map<long long, Map_value_type> m_value;
  /// Flag specifying whether this statistic is enabled, when false, methods
  /// are noop and return default values
  bool m_enabled{true};
};

}  // namespace mysql::scheduler

#endif  // MYSQL_SCHEDULER_SHARDED_COUNTER_H
