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

#ifndef MYSQL_SCHEDULER_DELAYED_SCHEDULE_H
#define MYSQL_SCHEDULER_DELAYED_SCHEDULE_H

#include <chrono>
#include <iostream>
#include <memory>
#include <string>

#include "mysql/scheduler/task_id.h"
#include "mysql/scheduler/task_schedule.h"
#include "mysql/scheduler/time.h"

namespace mysql::scheduler {

/// @class Delayed_schedule
/// @brief Represents Schedule for one-shot, delayed task
class Delayed_schedule : public Task_schedule {
 public:
  /// @brief Constructor
  /// @details constructs schedule with delay us from the clock start point
  /// @param task_id Task identifier
  /// @param clock Scheduler clock
  /// @param delay Delay in microseconds
  Delayed_schedule(Task_id task_id, Scheduler_clock_ptr clock, uint64_t delay);

  /// @brief This function modifies the state of the schedule to next state
  /// @returns false - Task is one-shot task, won't be executed in the future
  bool next() override;

  /// @brief Gets information about next execution time
  /// @returns Current phase task delay
  const Time_delay_type &get_task_delay() const override;

  bool is_finished() const override { return true; }

  /// @brief Obtain task clock
  const Scheduler_clock_ptr &get_clock() const override;

  /// @brief Obtain task id
  const Task_id &get_id() const override;

 protected:
 private:
  /// This task id
  Task_id m_task_id;
  /// Clock this schedule is based on
  Scheduler_clock_ptr m_clock;
  /// Delay since clock start point
  uint64_t m_task_delay;
};

}  // namespace mysql::scheduler

#endif  // MYSQL_SCHEDULER_DELAYED_SCHEDULE_H
