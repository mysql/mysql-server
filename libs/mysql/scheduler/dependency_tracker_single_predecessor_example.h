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

#ifndef MYSQL_SCHEDULER_DEPENDENCY_TRACKER_H
#define MYSQL_SCHEDULER_DEPENDENCY_TRACKER_H

#include <cassert>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "mysql/concurrency/condition_variable.h"
#include "mysql/concurrency/mutex.h"
#include "mysql/concurrency/spin_lock_mutex.h"
#include "mysql/concurrency/stage.h"
#include "mysql/scheduler/base_dependency_tracker.h"
#include "mysql/scheduler/dependency_tracker_psi.h"
#include "mysql/scheduler/task_id.h"
#include "mysql/scheduler/task_registry_multi.h"

namespace mysql::scheduler {

/// An example of Dependency Tracker implementation that tracks a single
/// task predecessor. Used only for excercising the tracker interface.
class Dependency_tracker_single_predecessor : public Base_dependency_tracker {
 public:
  using Task_id_type = Task_id;
  using Mutex_type = concurrency::Mutex;
  using Mt_key = concurrency::Mutex_key;
  using St_key = concurrency::Stage_key;

  /// Representation of a dependency
  struct Dependency {
    /// Task identifier
    Task_id task_id;
    /// Task predecessor
    std::optional<Task_id> predecessor;
    /// Task can have many successors
    std::vector<Task_id> successors;
  };

  Dependency_tracker_single_predecessor();

  /// @brief add_dependency
  /// @details Function adds dependency if valid
  /// @param predecessor Predecessor task ID
  /// @param successor Successor task ID
  /// @retval true Dependency added successfully
  /// @retval false Failed to add dependency
  [[nodiscard]] bool add_dependency(const Task_id_type &predecessor,
                                    const Task_id_type &successor) override;

  /// @brief Register task in the system to prevent adding dependencies on
  /// non-existing tasks and adding active dependencies on tasks that are
  /// already finished
  [[nodiscard]] bool activate_task(const Task_id &task) override;

  /// @brief check_ready
  /// @details Function checks whether a task is ready for execution, without
  /// locking the object (load)
  /// @param task Task ID
  /// @return true - dependencies of task are met, false - not met, second is
  [[nodiscard]] bool check_ready(const Task_id &task) override;

  /// @brief Task is finished, mark all dependencies of this task as met
  /// @param task Identifier of a task that finished its execution
  /// @param is_finished Whether the task has finished execution
  /// @return Vector of task IDs whose dependencies changed, since this class
  /// implementation supports 1 predecessor, this will be a vector of tasks
  /// IDs ready to execute
  [[nodiscard]] std::vector<Task_id> mark_dependency_met(
      const Task_id &task, bool is_finished) override;

  /// @brief Get the list of tasks that depend on the given task
  /// @param task The task ID
  /// @return Vector of successor task IDs
  std::vector<Task_id> get_successors(const Task_id &task);

 private:
  using Dependencies_registry = Task_registry_multi<Task_id, Dependency>;
  /// Registry of task dependencies. Concurrency is covered by internal
  /// representation of the registry.
  Dependencies_registry m_dependencies;
};

}  // namespace mysql::scheduler

#endif  // MYSQL_SCHEDULER_DEPENDENCY_TRACKER_H
