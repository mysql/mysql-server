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

#ifndef MYSQL_SCHEDULER_BASE_DEPENDENCY_TRACKER_H
#define MYSQL_SCHEDULER_BASE_DEPENDENCY_TRACKER_H

#include <cassert>
#include <condition_variable>
#include <list>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "mysql/concurrency/condition_variable.h"
#include "mysql/concurrency/mutex.h"
#include "mysql/concurrency/spin_lock_mutex.h"
#include "mysql/scheduler/task_id.h"
#include "mysql/scheduler/task_registry.h"

namespace mysql::scheduler {

/// @brief Base interface for a dependency tracker. Allows for:
/// - Registering a task in a dependency tracker (activate_task)
/// - Setting a dependency between one task and the other (add_dependency);
///   this method is typically called by a scheduling thread
/// - Checking if dependency of a task were already satisfied (check_ready);
///   this method is typically called by a worker / scheduling thread which
///   checks whether a task is ready to be executed
/// - Marking that dependencies of a concrete task were satisfied
///   (mark_dependency_met). This method is typically called by a worker that
///   finished executing its task
class Base_dependency_tracker {
 public:
  using Task_id_type = Task_id;
  using Mutex_type = concurrency::Mutex;

  /// @brief add_dependency
  /// @details Function adds dependency if valid
  /// @param predecessor Predecessor task ID
  /// @param successor Successor task ID
  /// @retval true Dependency added successfully
  /// @retval false Failed to add dependency
  [[nodiscard]] virtual bool add_dependency(const Task_id_type &predecessor,
                                            const Task_id_type &successor) = 0;

  /// @brief Register task in the system to prevent adding dependencies on
  /// non-existing tasks and adding active dependencies on tasks that are
  /// already finished
  [[nodiscard]] virtual bool activate_task(const Task_id &task) = 0;

  /// @brief check_ready
  /// @details Function checks whether a task is ready for execution
  /// @param task Task ID
  /// @return true - dependencies of task are met, false - not met, second is
  [[nodiscard]] virtual bool check_ready(const Task_id &task) = 0;

  /// @brief Task is finished, mark all dependencies of this task as met
  /// @param task Identifier of a task that finished its execution
  /// @param is_finished Whether the task has finished execution
  /// @return Vector of task IDs whose dependencies changed
  [[nodiscard]] virtual std::vector<Task_id> mark_dependency_met(
      const Task_id &task, bool is_finished) = 0;

  /// @brief Destructor
  virtual ~Base_dependency_tracker() = default;
};

using Dependency_tracker_ptr = std::unique_ptr<Base_dependency_tracker>;

}  // namespace mysql::scheduler

#endif  // MYSQL_SCHEDULER_DEPENDENCY_TRACKER_H
