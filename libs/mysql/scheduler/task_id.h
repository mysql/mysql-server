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

#ifndef MYSQL_SCHEDULER_TASK_ID_H
#define MYSQL_SCHEDULER_TASK_ID_H

#include <cassert>
#include <functional>
#include <future>
#include <iostream>
#include <memory>
#include <ostream>
#include <string>
#include <thread>

namespace mysql::scheduler {

/// @brief Represents the identifier of a task ingested by the scheduler,
/// @details Uses an internal sequence number for lightweight, format-agnostic
/// tracking.
class Task_id {
 public:
  /// @brief Creates scheduler task identifier using given id
  /// @note Uses an internal sequence number as a lightweight identifier,
  /// decoupling the scheduler from specific replication formats (e.g.,
  /// streaming where events may lack traditional identifiers or represent
  /// partial units).
  /// @param id Internally assigned sequence number
  Task_id(std::size_t id);

  /// @brief Compares tasks ids
  /// @param other Task id to compare against
  /// @return True if this task has lower number than other (was created before)
  bool operator<(const Task_id &other) const;

  /// @brief Returns an information on whether task id has been set to a valid
  /// id
  /// @return True in case identifier has been set; false otherwise
  bool is_valid() const { return m_is_valid; }

  uint64_t get() const { return m_id; }

  /// @brief Comparison operator required by unordered set/unordered map...
  bool operator==(const Task_id &src) const;

  friend class Scheduler;

  /// @brief Streaming operator
  friend std::ostream &operator<<(std::ostream &os, const Task_id &obj);

 protected:
 private:
  /// Internal transaction identifier
  std::size_t m_id;
  /// Id validity, false means that id has not been set
  bool m_is_valid = false;
};

}  // namespace mysql::scheduler

namespace std {
template <>
struct hash<mysql::scheduler::Task_id> {
  /// @brief Returns hash for Task_id
  /// @param id Task identifier
  size_t operator()(const mysql::scheduler::Task_id &id) const noexcept {
    assert(id.is_valid());
    return std::hash<std::size_t>()(id.get());
  }
};
}  // namespace std

#endif  // MYSQL_SCHEDULER_TASK_ID_H
