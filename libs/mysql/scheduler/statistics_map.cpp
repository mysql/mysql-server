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

#include "mysql/scheduler/statistics_map.h"

#include <cassert>
#include <iostream>
#include "mysql/scheduler/statistics_monitor.h"

namespace mysql::scheduler {

bool Statistics_map::init_statistics(std::size_t instance_id,
                                     std::size_t num_threads,
                                     bool enable_extended_statistics) {
  if (instance_id >= Constants::max_instances) {
    return true;
  }
  assert(instance_id < Constants::max_instances);
  auto &stat_monitor = mysql::scheduler::Statistics_monitor::get(instance_id);
  stat_monitor.init();
  stat_monitor.register_stat(Statistics_map::thp_queue_size, 1,
                             enable_extended_statistics);
  stat_monitor.register_stat(Statistics_map::thp_task_exec_time, num_threads,
                             true);
  stat_monitor.register_stat(Statistics_map::thp_worker_exec_time, num_threads,
                             true);
  stat_monitor.register_stat(Statistics_map::sched_task_exec_time, num_threads,
                             enable_extended_statistics);
  stat_monitor.register_stat(Statistics_map::thp_thread_internal_id,
                             num_threads, true);
  return false;
}

}  // namespace mysql::scheduler
