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

#ifndef MYSQL_SCHEDULER_SCHEDULED_TASK_H
#define MYSQL_SCHEDULER_SCHEDULED_TASK_H

#include <chrono>
#include <iostream>
#include <memory>
#include <string>

#include "mysql/scheduler/dispatch_reason.h"
#include "mysql/scheduler/task_id.h"
#include "mysql/scheduler/task_result.h"
#include "mysql/scheduler/task_schedule.h"
#include "mysql/scheduler/time.h"

namespace mysql::scheduler {

/// @class Repeatable_task_state
/// @brief Shared state for one logical repeatable scheduler task.
///
/// This object is shared by all scheduled representations of the same logical
/// task, including the currently queued phase, the task handed off to a worker,
/// and any follow-up phase enqueued by the scheduler.
///
/// It combines:
/// - the repeatable task callable, executed by each task phase
/// - the logical task lifetime accounting for @c m_scheduled_tasks_cnt
///
/// The scheduler increments @c m_scheduled_tasks_cnt once when the logical task
/// is created. The counter is decremented in this object's destructor, which
/// means the decrement happens only after the last queued or running owner of
/// the logical task releases its shared reference.
class Repeatable_task_state {
 public:
  using Repeatable_task_type = std::function<bool(unsigned int)>;

  /// @brief Creates shared state for a logical repeatable task.
  /// @param scheduled_tasks_cnt Reference to the scheduler task counter.
  /// @param repeatable_task Repeatable task callable shared across phases.
  Repeatable_task_state(std::atomic<std::size_t> &scheduled_tasks_cnt,
                        Repeatable_task_type &&repeatable_task);

  /// @brief Decrements the logical scheduled task count.
  ~Repeatable_task_state();

  Repeatable_task_state(const Repeatable_task_state &) = delete;
  Repeatable_task_state &operator=(const Repeatable_task_state &) = delete;

  /// @brief Executes the repeatable task body for one phase dispatch.
  /// @param thread_id Worker or scheduler thread identifier.
  /// @return True if task execution reported an error, false otherwise.
  bool execute(unsigned int thread_id) { return m_repeatable_task(thread_id); }

 private:
  /// Reference to the scheduler's logical task counter.
  std::atomic<std::size_t> &m_scheduled_tasks_cnt;
  /// Repeatable task body shared by all scheduled task instances.
  Repeatable_task_type m_repeatable_task;
};

using Repeatable_task_state_ptr = std::shared_ptr<Repeatable_task_state>;

/// @class Scheduled_task
/// @brief Represents task that is scheduled in the priority queue
class Scheduled_task {
  using Func_type = std::function<Task_result(unsigned int)>;

 public:
  /// @brief Constructor
  /// @param id Task ID
  /// @param task Function to execute
  /// @param repeatable_task Shared repeatable task state
  /// @param schedule Right reference to task schedule pointer
  /// @param phase_id Current phase sequence number
  Scheduled_task(const Task_id &id, Func_type &&task,
                 const Repeatable_task_state_ptr &repeatable_task,
                 const Task_schedule_ptr &schedule, unsigned int phase_id);

  /// @brief Returns task with a lower priority - operator needed for the
  /// priority queue
  bool operator<(const Scheduled_task &arg) const;

  /// @brief Gets the task delay time point.
  /// @return The time point for the task delay.
  Scheduler_clock::Time_point_t get_task_delay() const {
    return m_schedule->get_phase_delay(m_phase);
  }

  /// @brief Gets the task ID.
  /// @return The task ID.
  Task_id get_id() const { return m_task_id; }

  /// @brief Checks if the current phase is ready.
  /// @return true if phase is ready, false otherwise.
  bool is_phase_ready() const { return m_schedule->is_phase_ready(); }

  /// @brief Sets the repeatable task and marks as enqueued by scheduler.
  void set_enqueued_by_scheduler() { m_schedule->set_enqueued_by_scheduler(); }

  /// @brief Checks if enqueued by scheduler.
  /// @return true if enqueued by scheduler, false otherwise.
  bool is_enqueued_by_scheduler() {
    return m_schedule->is_enqueued_by_scheduler();
  }

  /// @brief Gets the shared repeatable task state that owns task lifetime.
  /// @return The shared repeatable task state pointer.
  Repeatable_task_state_ptr get_repeatable_task() { return m_repeatable_task; }

  /// @brief Gets the dispatch reason for this task instance.
  /// @return Dispatch reason assigned by the scheduler.
  Dispatch_reason get_dispatch_reason() const { return m_dispatch_reason; }

  /// @brief Sets the dispatch reason for this task instance.
  /// @param dispatch_reason Reason assigned by the scheduler.
  void set_dispatch_reason(Dispatch_reason dispatch_reason) {
    m_dispatch_reason = dispatch_reason;
  }

  friend class Scheduler;

 protected:
  /// Task identifier (local to scheduler)
  Task_id m_task_id;
  /// Task function
  Func_type m_task;
  /// Shared repeatable task state carrying execution and logical lifetime.
  Repeatable_task_state_ptr m_repeatable_task;
  /// Task schedule
  Task_schedule_ptr m_schedule;
  /// Current phase counter
  int m_phase{0};
  /// Dispatch reason assigned when task is released to the worker pool.
  Dispatch_reason m_dispatch_reason{Dispatch_reason::normal};
};

}  // namespace mysql::scheduler

#endif  // MYSQL_SCHEDULER_SCHEDULED_TASK_H
