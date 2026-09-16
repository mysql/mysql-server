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

#ifndef MYSQL_SCHEDULER_COMMIT_ORDER_CLOCK_H
#define MYSQL_SCHEDULER_COMMIT_ORDER_CLOCK_H

#include <atomic>
#include <cstdint>
#include <functional>
#include <limits>
#include <ostream>

#include "mysql/concurrency/condition_variable.h"
#include "mysql/scheduler/scheduler_clock.h"

namespace mysql::scheduler {

class Commit_order_clock : public Scheduler_clock {
 public:
  using Time_point_t = uint64_t;

  /// Constructor
  /// @param start_point Clock start point (set as now time)
  Commit_order_clock(Time_point_t start_point = 0);

  /// @brief Gets clock start time
  /// @return Start time
  Time_point_t start_time() const override;

  /// @brief Gets current time, i.e. current processing window value
  /// @return Current time, i.e. current processing window value
  Time_point_t now() const override;

  /// @brief Subscribes a task to \a time processing window
  /// @param task_id Unused task ID
  /// @param time Time window to which a task will be subscribed
  /// @return True if task has been subscribed, false otherwise
  bool add_time(Task_id task_id, Time_point_t time) override;

  /// @brief Notify that task finished execution. Called by asynchronous workers
  /// executing task. When specific conditions are met, tick will advance the
  /// internal clock value.
  /// @param task_id Unused task ID
  /// @param time Delay value the task was subscribed to
  /// @return True if time advanced, false otherwise
  bool tick(Task_id task_id, Time_point_t time) override;

  /// @brief This clock dependency type is start-to-start
  Scheduler_dependency_type get_type() const override {
    return Scheduler_dependency_type::start_to_start;
  }

 private:
  /// Current clock value (now / delay from the start point)
  std::atomic<Time_point_t> m_clock{0};
  /// Used to track stalled clock unblocking (clock was twicked by this count)
  std::atomic<std::size_t> m_twicked_count{0};
};

}  // namespace mysql::scheduler

#endif  // MYSQL_SCHEDULER_COMMIT_ORDER_CLOCK_H
