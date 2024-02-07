/*
 * Copyright (c) 2020, 2024, Oracle and/or its affiliates.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is designed to work with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have either included with
 * the program or referenced in the documentation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License, version 2.0, for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
 */

#ifndef PLUGIN_X_SRC_SERVER_SCHEDULER_MONITOR_H_
#define PLUGIN_X_SRC_SERVER_SCHEDULER_MONITOR_H_

#include "plugin/x/src/interface/scheduler_dynamic.h"
#include "plugin/x/src/variables/xpl_global_status_variables.h"

namespace xpl {

class Worker_scheduler_monitor : public iface::Scheduler_dynamic::Monitor {
 public:
  void on_worker_thread_create() override {
    ++xpl::Global_status_variables::instance().m_worker_thread_count;
  }

  void on_worker_thread_destroy() override {
    --xpl::Global_status_variables::instance().m_worker_thread_count;
  }

  void on_task_start() override {
    ++xpl::Global_status_variables::instance().m_active_worker_thread_count;
  }

  void on_task_end() override {
    --xpl::Global_status_variables::instance().m_active_worker_thread_count;
  }
};

}  // namespace xpl

#endif  // PLUGIN_X_SRC_SERVER_SCHEDULER_MONITOR_H_
