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

#ifndef MYSQL_CSA_DEPENDENCY_ADAPTER_H
#define MYSQL_CSA_DEPENDENCY_ADAPTER_H

#include <cstdint>
#include <memory>
#include <optional>
#include "mysql/scheduler/task_id.h"

namespace mysql::csa {

class Dependency_adapter;
/// @brief Unique pointer type for Dependency_adapter
using Dependency_adapter_ptr = std::unique_ptr<Dependency_adapter>;

/// @brief Class that resolves dependencies based on transaction sequence
/// number and last committed. It returns id of dependent task for a
/// dependency tracker in the scheduler
class Dependency_adapter {
 public:
  /// @brief Alias for task identifier from scheduler
  using Task_id = mysql::scheduler::Task_id;
  /// @brief Optional resolved task identifier
  using Task_id_resolved = std::optional<Task_id>;
  /// @brief Optional clock delay value
  using Clock_delay = std::optional<uint64_t>;

  /// @brief Virtual destructor
  virtual ~Dependency_adapter() = default;

  /// @brief Remember the worker pool size if needed. Default - unused
  virtual void set_worker_num(uint32_t) {}

  /// @brief Solves dependencies - figures out after which task a task with
  /// the given id should run
  /// @param id Task id
  /// @param seq_num Transaction sequence number
  /// @param commit_parent Transaction commit parent (LC)
  /// @return A pair containing an optional clock delay and an optional resolved
  /// task ID
  virtual std::pair<Clock_delay, Task_id_resolved> solve(
      Task_id id, int64_t seq_num, int64_t commit_parent) = 0;
};

}  // namespace mysql::csa

#endif  // MYSQL_CSA_DEPENDENCY_ADAPTER_H
