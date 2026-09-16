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

#ifndef MYSQL_SCHEDULER_STATISTICS_MONITOR_H
#define MYSQL_SCHEDULER_STATISTICS_MONITOR_H

#include <array>
#include <atomic>
#include <chrono>
#include <memory>
#include <new>
#include <optional>
#include <unordered_map>
#include "mysql/concurrency/condition_variable.h"
#include "mysql/concurrency/mutex.h"
#include "mysql/scheduler/constants.h"
#include "mysql/scheduler/sharded_counter.h"
#include "mysql/scheduler/statistics_instance_monitor.h"

namespace mysql::scheduler {

/// @brief Singleton Statistics Monitor for all of the registered "instances".
/// We typically track statistics separately for user-defined channels
class Statistics_monitor {
 public:
  using Mutex = concurrency::Mutex;
  using Mutex_key = concurrency::Mutex_key;

  static Statistics_instance_monitor &get(std::size_t instance_id);
  static void clear(std::size_t instance_id);

 protected:
  /// @brief This initialization function is called by "get" if needed
  static void init();
  Statistics_monitor() = default;
  using Statistics_instance_monitor_ptr =
      std::unique_ptr<Statistics_instance_monitor>;
  using Instances_map =
      std::array<Statistics_instance_monitor, Constants::max_instances>;
  static Instances_map &instances();
  static std::atomic<bool> m_init;
  static std::atomic<bool> m_ready;
};

}  // namespace mysql::scheduler

#endif  // MYSQL_SCHEDULER_STATISTICS_MONITOR_H
