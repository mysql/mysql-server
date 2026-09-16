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

#include "mysql/scheduler/dependency_tracker_single_predecessor_example.h"
#include "mysql/scheduler/task_registry_multi.h"

namespace mysql::scheduler {

Dependency_tracker_single_predecessor::Dependency_tracker_single_predecessor()
    : m_dependencies(Dependencies_registry::default_capacity) {}

bool Dependency_tracker_single_predecessor::add_dependency(
    const Task_id_type &predecessor, const Task_id_type &successor) {
  // Check if predecessor is active
  bool pred_active = m_dependencies.apply(predecessor, [](Dependency &) {});
  if (pred_active) {
    // Set predecessor for successor
    std::ignore = m_dependencies.apply(
        successor, [&](Dependency &dep) { dep.predecessor = predecessor; });
    // Add successor to predecessor's successors
    std::ignore = m_dependencies.apply(predecessor, [&](Dependency &dep) {
      dep.successors.push_back(successor);
    });
  }

  return true;
}

bool Dependency_tracker_single_predecessor::activate_task(const Task_id &task) {
  return m_dependencies.activate(task, Dependency{task, {}, {}});
}

bool Dependency_tracker_single_predecessor::check_ready(const Task_id &task) {
  std::optional<Task_id> predecessor;
  bool applied = m_dependencies.apply(
      task, [&](Dependency &dep) { predecessor = dep.predecessor; });
  if (!applied || !predecessor) {
    return true;
  }
  // If predecessor is not active, it's finished
  bool pred_active =
      m_dependencies.apply(predecessor.value(), [](Dependency &) {});
  return !pred_active;
}

std::vector<Task_id> Dependency_tracker_single_predecessor::mark_dependency_met(
    const Task_id &task, [[maybe_unused]] bool is_finished) {
  assert(is_finished);
  std::vector<Task_id> newly_ready;
  std::ignore = m_dependencies.apply(
      task, [&](Dependency &dep) { newly_ready = std::move(dep.successors); });
  [[maybe_unused]] bool deactivated = m_dependencies.deactivate(task);
  assert(deactivated);
  return newly_ready;
}

std::vector<Task_id> Dependency_tracker_single_predecessor::get_successors(
    const Task_id &task) {
  std::vector<Task_id> successors;
  std::ignore = m_dependencies.apply(
      task, [&](Dependency &dep) { successors = dep.successors; });
  return successors;
}

}  // namespace mysql::scheduler
