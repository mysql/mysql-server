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

#ifndef MYSQL_SCHEDULER_TASK_SCHEDULE_H
#define MYSQL_SCHEDULER_TASK_SCHEDULE_H

#include <atomic>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <memory>
#include <string>

#include "mysql/scheduler/scheduler_clock.h"
#include "mysql/scheduler/task_id.h"
#include "mysql/scheduler/time.h"

namespace mysql::scheduler {

class Task_schedule;
using Task_schedule_ptr = std::shared_ptr<Task_schedule>;

/// @class Task_schedule
/// @brief Represents task schedule. This is the base class for a schedule
class Task_schedule {
 public:
  /// @brief Constructor sets the task delay to a given value
  Task_schedule();

  /// @brief Compare two task schedules to indicate which task should run
  /// first
  /// @details Checks whether THIS task has higher priority than a given
  /// task in terms of time left to the execution
  /// @param arg Task to compare against
  /// @param phase Compares task priority for a given phase (phase seqnence
  /// number)
  /// @returns true - this task has higher priority than arg task, false -
  /// this task has lower or equal priority than given task
  bool has_higher_priority(const Task_schedule &arg, int phase) const;

  /// @brief Determines whether to re-execute this schedule
  /// @details This function modifies the state of the schedule to next state
  /// @returns true - Task will be executed in the future, false - task
  /// fulfilled its schedule and won't be executed
  /// @note TBI in derived
  virtual bool next() = 0;

  /// @brief Obtain task clock
  virtual const Scheduler_clock_ptr &get_clock() const = 0;

  /// @brief Obtain task id
  virtual const Task_id &get_id() const = 0;

  /// @brief Obtains task execution point as a delay from the clock start
  /// @details Gets information about task execution time - task delay since
  /// the beginning of a schedule
  /// @note TBI in derived
  virtual const Time_delay_type &get_task_delay() const = 0;

  /// @note TBI in derived
  virtual bool is_finished() const = 0;

  /// Destructor
  virtual ~Task_schedule() = default;

  /// @brief Obtain task clock for the given phase (current one by default)
  /// @return Phase clock reference
  virtual const Scheduler_clock_ptr &get_phase_clock(unsigned int) const {
    return get_clock();
  }

  /// @brief Obtains task execution point as a delay from the clock start, but
  /// for the given phase (current one by default)
  /// @details Gets information about task execution time - task delay since
  /// the beginning of a schedule
  /// @note TBI in derived
  virtual const Time_delay_type &get_phase_delay(unsigned int) const {
    return get_task_delay();
  }

  /// @brief Returns current phase id (sequence number), 0 by default
  /// @return Phase sequence number
  virtual unsigned int get_phase_id() const { return 0; }

  /// @brief Sets the flag indicating that the task was enqueued by the
  /// scheduler.
  void set_enqueued_by_scheduler() { m_enqueued_by_worker = false; }

  /// @brief Checks if the task was enqueued by the scheduler.
  /// @return true if enqueued by the scheduler, false otherwise.
  bool is_enqueued_by_scheduler() const { return !m_enqueued_by_worker; }

  /// @brief Checks if the current phase is ready for execution.
  /// @return true if phase is ready, false otherwise.
  bool is_phase_ready() const { return m_phase_ready.load(); }

  /// @brief Sets the phase ready flag to true.
  void set_phase_ready() { m_phase_ready.store(true); }

 private:
  /// Flag indicating that a phase is ready to be executed
  std::atomic<bool> m_phase_ready{false};
  /// Flag inicating that phase will be enqueued by worker
  bool m_enqueued_by_worker{true};
};

}  // namespace mysql::scheduler

#endif  // MYSQL_SCHEDULER_TASK_SCHEDULE_H
