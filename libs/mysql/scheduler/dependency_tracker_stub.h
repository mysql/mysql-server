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

#ifndef MYSQL_SCHEDULER_DEPENDENCY_TRACKER_STUB_H
#define MYSQL_SCHEDULER_DEPENDENCY_TRACKER_STUB_H

#include "mysql/concurrency/condition_variable.h"
#include "mysql/scheduler/base_dependency_tracker.h"

namespace mysql::scheduler {

/// @brief Dependency tracker stub: empty tracker, does not check dependencies
class Dependency_tracker_stub : public Base_dependency_tracker {
 public:
  bool add_dependency(const Task_id_type &, const Task_id_type &) override {
    return true;
  }
  bool activate_task(const Task_id &) override { return true; }
  bool check_ready(const Task_id &) override { return true; }
  std::vector<Task_id> mark_dependency_met(const Task_id &, bool) override {
    return {};
  }
  virtual ~Dependency_tracker_stub() override = default;
};

}  // namespace mysql::scheduler

#endif  // MYSQL_SCHEDULER_DEPENDENCY_TRACKER_STUB_H
