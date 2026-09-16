// Copyright (c) 2026, Oracle and/or its affiliates.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License, version 2.0,
// as published by the Free Software Foundation.
//
// This program is distributed to work with certain software (including
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

#ifndef MYSQL_SCHEDULER_CLOCK_LWM_REGISTRY_H
#define MYSQL_SCHEDULER_CLOCK_LWM_REGISTRY_H

#include <atomic>
#include <memory>
#include "mysql/scheduler/scheduler_clock.h"
#include "mysql/scheduler/scheduler_clock_psi.h"
#include "mysql/scheduler/task_id.h"
#include "mysql/scheduler/task_registry_multi.h"

namespace mysql::scheduler {

/// @brief Clock implementation that computes LWM (Low Water Mark) based on
/// executed tasks. This class provides a Scheduler_clock interface where the
/// "time" is represented by the LWM, which can be defined in two equivalent
/// ways:
/// - The maximum task ID such that the task and all preceding tasks are
/// completed.
/// - The minimum task ID that is not completed, minus 1.
///
/// Internally, this class uses a Task_registry_multi to track the state of
/// tasks (registered, started, finished). It maintains an atomic LWM value,
/// updated when tasks finish execution via the tick() method. The LWM advances
/// to the maximum ID where all tasks up to that ID are finished.
///
/// Key Identifiers (Internal to this Class):
/// - Task_id: Opaque ID used in the Scheduler_clock API and internally.
///   Assigned by the scheduler or caller; this class does not assign or
///   translate IDs.
/// - LWM (Low Water Mark): The current "time" value, starting at 0 and
/// monotonically increasing.
///
/// Performance Characteristics:
/// - Uses fine-grained locking with 16384 buckets in the Task_registry_multi.
/// - Employs spin locks for low-latency synchronization in low-contention
/// scenarios.
///
/// API Overview:
/// - now(): Returns the current LWM.
/// - start_time(): Returns 0 (initial LWM).
/// - add_time(task_id, time_point): Registers a task at the given task_id. The
/// time_point is ignored here.
/// - tick(task_id, time_point): Marks the task as finished and updates LWM if
/// possible. The time_point is ignored.
/// - The class handles concurrency via atomics for LWM and per-bucket locking
/// in the registry.
///
/// Relation to External Components (e.g., CSA):
/// This class is designed for use in systems like Change Stream Apply (CSA),
/// where:
/// - CSA assigns Task_id identifiers to received transactions and computes a
/// last committed task ID (the ID of the last transaction that must precede
/// it).
/// - Translation from source's parallel indexes to Task_id occurs outside this
/// class (in CSA logic).
/// - A CSA task registers with add_time and waits until now() >= it's LWM time
/// - Upon finishing, it calls tick( on its task ID ).
/// - This ensures causality: a transaction applies only after all tasks up to
/// its LWM time has completed
/// - LWM enables parallelism: tasks with higher IDs can proceed if LWM advances
/// past lower ones.
class Clock_lwm_registry : public Scheduler_clock {
 public:
  /// @brief Construct the clock - initializes the internal task registry
  /// Since we use spin lock in this implementation, we skip PSI instrumentation
  /// @param clock_capacity The maximum number of tasks this clock may
  /// handle
  /// @param psi_params Instrumentation
  Clock_lwm_registry(std::size_t clock_capacity = 8192,
                     [[maybe_unused]] Scheduler_clock_psi psi_params = {})
      : m_executed_registry(clock_capacity) {}

  /// @brief Get the current value of the LWM
  /// @return Current time (LWM)
  Time_point_t now() const override;

  /// @brief Get start time of this clock - the first LWM value (0)
  /// @return Clock start time for the scheduler to know from which time
  /// we calculate the task delay
  Time_point_t start_time() const override;

  /// @brief Registers a task to execute at specific LWM. Here, we don't need
  /// to know at which LWM task executes.
  /// @param task_id Id of the task that finished execution
  /// @return True if task was registered successfully, false otherwise.
  bool add_time(Task_id task_id, Time_point_t) override;

  /// @brief Tick performed on the clock when task finishes
  /// @param task_id Id of the task that finished execution
  /// @return True if clock value changed, false otherwise.
  bool tick(Task_id task_id, Time_point_t) override;

  /// @brief Backdoor for unit tests to set LWM to a specific value
  void test_set_current_lwm(Time_point_t lwm);

 private:
  /// This structure is used to represent a registered task state and update
  /// LWM. After registration, the task stays active as long as LWM is lower
  /// than the task ID. After LWM passes its ID (finished is equal to true),
  /// task may be unregistered from the registry.
  struct Task_state {
    /// @brief Task has started
    bool started = false;
    /// @brief Task has finished
    bool finished = false;
  };

  /// @brief Registry of executed tasks. Concurrency is covered by the
  /// Task Registry Multi (handles id conflicts)
  Task_registry_multi<Task_id, Task_state> m_executed_registry;
  /// @brief Current LWM value
  std::atomic<Time_point_t> m_current_lwm{0};
};

}  // namespace mysql::scheduler

#endif  // MYSQL_SCHEDULER_CLOCK_LWM_REGISTRY_H
