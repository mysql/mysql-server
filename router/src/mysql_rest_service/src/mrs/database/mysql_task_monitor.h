/*
 * Copyright (c) 2025, 2026, Oracle and/or its affiliates.
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
 * This program is distributed in the hope that it will be useful,  but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
 * the GNU General Public License, version 2.0, for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef ROUTER_SRC_MYSQL_REST_SERVICE_SRC_MRS_DATABASE_MYSQL_TASK_MONITOR_H_
#define ROUTER_SRC_MYSQL_REST_SERVICE_SRC_MRS_DATABASE_MYSQL_TASK_MONITOR_H_

#include <list>
#include <memory>
#include <string>
#include <thread>

#include "collector/mysql_fixed_pool_manager.h"

#include "mysql/harness/stdx/monitor.h"
#include "mysql/harness/utility/wait_variable.h"
#include "mysqlrouter/utils_sqlstring.h"

namespace mrs {
namespace database {

class MysqlTaskMonitor {
 public:
  using CachedSession = collector::MysqlFixedPoolManager::CachedObject;
  using PoolManager = collector::MysqlFixedPoolManager;
  using PoolManagerRef = std::shared_ptr<PoolManager>;

 public:
  ~MysqlTaskMonitor();

  void call_async(
      CachedSession session, PoolManagerRef session_pool,
      std::list<std::string> preamble, std::string script,
      std::list<std::string> postamble,
      std::function<std::list<std::string>(const std::exception &)> on_error,
      const std::string &task_id);

 public:
  void start();
  void stop();
  void reset();

 private:
  template <typename T>
  using WaitableVariable = mysql_harness::utility::WaitableVariable<T>;

  struct Task {
    // holds a ref to the pool that owns session, so it's not released
    // while the Task is executing in another thread
    PoolManagerRef session_pool_ref;
    CachedSession session;
    std::list<std::string> preamble;
    std::string script;
    std::list<std::string> postamble;
    std::list<std::string> error;
    std::function<std::list<std::string>(const std::exception &)> on_error;

    std::string task_id;
    bool failed = false;
  };

  enum State { k_initializing, k_running, k_check_tasks, k_stopped };

  std::thread thread_;

  WaitableVariable<State> state_{k_initializing};

  std::mutex tasks_mutex_;
  std::list<Task> tasks_;

  void run();

  bool update_task(Task &task);
};

}  // namespace database
}  // namespace mrs

#endif  // ROUTER_SRC_MYSQL_REST_SERVICE_SRC_MRS_DATABASE_MYSQL_TASK_MONITOR_H_
