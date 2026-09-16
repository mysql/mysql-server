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

#ifndef MYSQL_SCHEDULER_DEPENDENCY_TRACKER_PSI_H
#define MYSQL_SCHEDULER_DEPENDENCY_TRACKER_PSI_H

#include "mysql/allocators/memory_resource.h"
#include "mysql/concurrency/condition_variable.h"
#include "mysql/concurrency/mutex.h"
#include "mysql/concurrency/thread.h"

namespace mysql::scheduler {

/// @brief Dependency_tracker instrumentation parameters, packed into this
/// structure to simplify construction of a Dependency_tracker class instance
/// and prevent mistakes when passing a lot of integer parameters into the class
/// constructor
struct Dependency_tracker_psi {
  /// mutex: One key for mutexes used to synchronize access to each entry (1
  /// mutex per 1 entry)
  concurrency::Mutex_key key_mt_registry_entry{0};
  /// stage: register / activate a task
  concurrency::Stage_key key_stage_activate_task{0};
  /// stage: add dependency
  concurrency::Stage_key key_stage_add_dependency{0};
  /// stage: check if task is ready to execute
  concurrency::Stage_key key_stage_check_ready{0};
  /// stage: mark dependency met (task callback)
  concurrency::Stage_key key_stage_mark_dependency_met{0};
  /// memory: memory instrumentation object
  allocators::Memory_resource memory_resource;
};

}  // namespace mysql::scheduler

#endif  // MYSQL_SCHEDULER_DEPENDENCY_TRACKER_PSI_H
