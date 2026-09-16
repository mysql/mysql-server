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

#ifndef MYSQL_SCHEDULER_SCHEDULER_PSI_H
#define MYSQL_SCHEDULER_SCHEDULER_PSI_H

#include "mysql/allocators/memory_resource.h"
#include "mysql/concurrency/condition_variable.h"
#include "mysql/concurrency/mutex.h"
#include "mysql/concurrency/thread.h"

namespace mysql::scheduler {

/// @brief Scheduler instrumentation parameters, packed into this
/// structure to simplify construction of a Scheduler class instance and
/// prevent mistakes when passing a lot of integer parameters into the
/// class constructor
struct Scheduler_psi {
  /// mutex: Key for mutex associated with scheduler condition variable
  concurrency::Mutex_key key_mt_sched{0};
  /// mutex: Key for mutex associated with cv used to end execution
  concurrency::Mutex_key key_mt_end{0};
  /// cv: Key for cv used by scheduler thread for all notifications
  concurrency::Cv_key key_cv_sched{0};
  /// cv: Key for cv ending execution
  concurrency::Cv_key key_cv_end{0};
  /// mutex: Key for mutex protecting enqueued tasks
  concurrency::Mutex_key key_mt_tasks{0};
  /// mutex: Key for mutex protecting phase queues
  concurrency::Mutex_key key_mt_phases{0};
  /// thread: Scheduler thread key
  concurrency::Thread_key key_th_scheduler{0};
  /// stage: stopping scheduler
  concurrency::Stage_key key_stage_stopping{0};
  /// stage: waiting for work
  concurrency::Stage_key key_stage_waiting{0};
  /// stage: scheduler thread stopped
  concurrency::Stage_key key_stage_stopped{0};
  /// stage: check dependencies
  concurrency::Stage_key key_stage_check_dependencies{0};
  /// stage: check stage queues
  concurrency::Stage_key key_stage_check_stage_queues{0};
  /// stage: enqueue ready tasks
  concurrency::Stage_key key_stage_enqueueing_ready_tasks{0};
  /// stage: waiting for clock (cannot handle more dependencies)
  concurrency::Stage_key key_stage_wait_clock_queue{0};
  /// stage: synchronizing scheduler - reached max task limit
  concurrency::Stage_key key_reached_max_task_limit{0};
  /// memory: memory instrumentation object
  allocators::Memory_resource memory_resource;
};

}  // namespace mysql::scheduler

#endif  // MYSQL_SCHEDULER_SCHEDULER_PSI_H
