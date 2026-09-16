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

#ifndef MYSQL_SCHEDULER_SCHEDULER_CLOCK_H
#define MYSQL_SCHEDULER_SCHEDULER_CLOCK_H

#include <cstdint>
#include <limits>
#include <memory>
#include <ostream>
#include "mysql/scheduler/task_id.h"

namespace mysql::scheduler {

class Scheduler_clock;
using Scheduler_clock_ptr = std::shared_ptr<Scheduler_clock>;

enum class Scheduler_dependency_type { end_to_start, start_to_start };

/// When unblocking, we tweak clock by this count
inline constexpr std::size_t clock_unblock_delta{4};

/// @brief Interface for clock implementations scheduler uses to
/// schedule tasks.
/// @details
/// Tasks are subscribing for execution at specific time window by calling
/// the 'subscribe' method.
/// A task is ready for execution, when its execution time, i.e.
/// parameter of the 'subscribe' method, is equal or greater than current time
/// point, determined by the 'now' method.
/// When task finished its execution, it must call the 'tick' method.
class Scheduler_clock {
 public:
  using Time_point_t = uint64_t;

  /// Destructor
  virtual ~Scheduler_clock() = default;

  /// @brief Gets current time delay, i.e. current processing window value
  /// @return Current time, i.e. current processing window value
  virtual Time_point_t now() const = 0;

  /// @brief Gets clock starting point, i.e. first delay
  /// @return clock start point
  virtual Time_point_t start_time() const = 0;

  /// @brief Subscribes a task to \a time processing window, delay since
  /// beginning of a schedule
  /// @param task_id ID of the task being subscribed
  /// @param time Time window to which a task will be subscribed
  /// @return True if task has been subscribed, false otherwise
  virtual bool add_time(Task_id task_id, Time_point_t time) = 0;

  /// @brief Notify that task finished execution. Called by asynchronous workers
  /// executing task. When specific conditions are met, tick will advance the
  /// internal clock value.
  /// @param task_id ID of the task that finished execution
  /// @param time Delay value the task was subscribed to
  /// @return True if clock advanced its time
  virtual bool tick(Task_id task_id, Time_point_t time) = 0;

  /// @brief Returns property of a clock - information on whether this clock
  /// is steered by task ticks or advances independently
  virtual bool advances_independently() const { return false; }

  /// @brief Maintenance function for unblocking the clock
  virtual bool try_unblock() { return true; }

  /// @brief Typical scheduler clock dependency type is end to start
  virtual Scheduler_dependency_type get_type() const {
    return Scheduler_dependency_type::end_to_start;
  }
};

}  // namespace mysql::scheduler

#endif  // MYSQL_SCHEDULER_SCHEDULER_CLOCK_H
