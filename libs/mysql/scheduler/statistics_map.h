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

#ifndef MYSQL_SCHEDULER_STATISTICS_MAP_H
#define MYSQL_SCHEDULER_STATISTICS_MAP_H

#include <atomic>
#include <thread>
#include <unordered_map>
#include "mysql/scheduler/constants.h"

namespace mysql::scheduler {

/// Supported statistics:
/// * thp_queue_size - current size of the thread pool queue size
/// * thp_task_exec_time - the number of microseconds spent by workers to
///   execute tasks
/// * thp_worker_exec_time - the number of microseconds spent to by thread pool
///   workers on waiting for tasks and execute tasks
/// * sched_task_exec_time - the number of microseconds spent by workers to
///   execute tasks, without callback overhead
/// * thp_thread_internal_id - thread internal id, set once after thread starts
class Statistics_map {
 public:
  static constexpr auto thp_queue_size = "thp_queue_size";
  static constexpr auto thp_task_exec_time = "thp_task_exec_time";
  static constexpr auto thp_worker_exec_time = "thp_worker_exec_time";
  static constexpr auto sched_task_exec_time = "sched_task_exec_time";
  static constexpr auto thp_thread_internal_id = "thp_thread_internal_id";
  [[nodiscard]] static bool init_statistics(
      std::size_t instance_id,
      std::size_t num_threads = Constants::max_thread_count,
      bool enable_extended_statistics = false);
};

}  // namespace mysql::scheduler

#endif  // MYSQL_SCHEDULER_STATISTICS_MAP_H
