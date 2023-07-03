/*
 * Copyright (c) 2020, 2022, Oracle and/or its affiliates.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is also distributed with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have included with MySQL.
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

#ifndef PLUGIN_X_SRC_INTERFACE_SCHEDULER_DYNAMIC_H_
#define PLUGIN_X_SRC_INTERFACE_SCHEDULER_DYNAMIC_H_

namespace xpl {
namespace iface {

// Scheduler with dynamic thread pool.
class Scheduler_dynamic {
 public:
  class Monitor {
   public:
    virtual ~Monitor() = default;

    virtual void on_worker_thread_create() = 0;
    virtual void on_worker_thread_destroy() = 0;
    virtual void on_task_start() = 0;
    virtual void on_task_end() = 0;
  };

  virtual ~Scheduler_dynamic() = default;

  virtual void launch() = 0;
  virtual void stop() = 0;
  virtual unsigned int set_num_workers(unsigned int n) = 0;
  virtual bool thread_init() = 0;
  virtual void thread_end() = 0;
};

}  // namespace iface
}  // namespace xpl

#endif  // PLUGIN_X_SRC_INTERFACE_SCHEDULER_DYNAMIC_H_
