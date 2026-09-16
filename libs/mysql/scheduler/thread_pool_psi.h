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

#ifndef MYSQL_SCHEDULER_THREAD_POOL_PSI_H
#define MYSQL_SCHEDULER_THREAD_POOL_PSI_H

#include "mysql/allocators/memory_resource.h"
#include "mysql/concurrency/condition_variable.h"
#include "mysql/concurrency/mutex.h"
#include "mysql/concurrency/thread.h"

namespace mysql::scheduler {

/// @brief Thread_pool instrumentation parameters, packed into this
/// structure to simplify construction of a Thread_pool class instance and
/// prevent mistakes when passing a lot of integer parameters into the
/// class constructor
struct Thread_pool_psi {
  /// thread: worker thread key
  concurrency::Thread_key key_thread_worker{0};
  /// stage: waiting for work
  concurrency::Stage_key key_stage_waiting{0};
  /// stage: executing task
  concurrency::Stage_key key_stage_executing{0};
  /// stage: thread stopped
  concurrency::Stage_key key_stage_stopped{0};
  /// memory: memory instrumentation object (thread pool queue)
  allocators::Memory_resource memory_resource{};
};

}  // namespace mysql::scheduler

#endif  // MYSQL_SCHEDULER_THREAD_POOL_PSI_H
