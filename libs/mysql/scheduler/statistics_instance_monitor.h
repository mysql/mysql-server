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

#ifndef MYSQL_SCHEDULER_STATISTICS_INSTANCE_MONITOR_H
#define MYSQL_SCHEDULER_STATISTICS_INSTANCE_MONITOR_H

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <optional>
#include <unordered_map>
#include "mysql/concurrency/condition_variable.h"
#include "mysql/concurrency/mutex.h"
#include "mysql/scheduler/constants.h"
#include "mysql/scheduler/sharded_counter.h"
#include "mysql/scheduler/statistics_type_traits.h"

namespace mysql::scheduler {

class Statistics_instance_monitor;
using Statistics_instance_monitor_ref =
    std::reference_wrapper<Statistics_instance_monitor>;

/// @brief CSA statistics monitor, gathers statistics coming from different
/// CSA threads
/// Each statistic is a pair of key (statistic string) and value. This will
/// help with backward and forward compatibility of statistics implemented
/// in CSA servis w.r.t. statistics supported in the server
/// Statistic update method ingests thread id, to keep a sharded statistics
/// value until coalescing operation requested by external thread to
/// get accumulated value (if coalescing is requested during "get" operation).
/// For the time being, we assume only 1 channel
class Statistics_instance_monitor {
 public:
  using Mutex = concurrency::Mutex;
  using Mutex_key = concurrency::Mutex_key;

  void init();

  /// @brief Get statistic value
  /// @param name Name of statistic
  /// @param thread_num The number of threads accessing statistic
  /// @param enable Information on whether to enable this particular statistic
  template <Statistic_allowed_type Type = long long>
  void register_stat(const std::string &name, std::size_t thread_num = 1,
                     bool enable = true);
  /// @brief Get accumulated statistic value
  /// @param name Name of statistic
  /// @return Value if statistic is found, no value otherwise
  template <Statistic_allowed_type Type = long long>
  std::optional<
      std::reference_wrapper<typename Statistic_type_traits<Type>::type>>
  find_stat(const std::string &name);
  /// @brief Get accumulated statistic value w/o bound checking
  /// @param name Name of statistic
  /// @return Value if statistic is found, no value otherwise w/o bound checking
  template <Statistic_allowed_type Type = long long>
  typename Statistic_type_traits<Type>::type &get_stat(const std::string &name);

  /// @brief Resets initialized statistics, use wisely
  void reset();
  /// @brief Frees registered statistics storage for this instance
  void clear();

  Statistics_instance_monitor() = default;

 protected:
  using Registry_type = std::unordered_map<std::string, Sharded_counter>;
  Registry_type m_registry;
};

}  // namespace mysql::scheduler

#include "mysql/scheduler/statistics_instance_monitor_impl.hpp"

#endif  // MYSQL_SCHEDULER_STATISTICS_INSTANCE_MONITOR_H
