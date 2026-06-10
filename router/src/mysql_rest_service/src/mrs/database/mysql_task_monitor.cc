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

#include "mrs/database/mysql_task_monitor.h"
#include <mysql.h>
#include <chrono>
#include "my_thread.h"  // NOLINT(build/include_subdir)
#include "mysql/harness/logging/logging.h"
#include "mysqlrouter/utils_sqlstring.h"

IMPORT_LOG_FUNCTIONS()

namespace mrs {
namespace database {

MysqlTaskMonitor::~MysqlTaskMonitor() { stop(); }

void MysqlTaskMonitor::start() {
  log_debug("MysqlTaskMonitor::%s", __FUNCTION__);
  thread_ = std::thread([this]() { run(); });

  state_.wait({k_running});
}

void MysqlTaskMonitor::stop() {
  if (state_.exchange({k_initializing, k_running}, k_stopped)) {
    log_debug("MysqlTaskMonitor::%s", __FUNCTION__);
  }

  // The thread might be already stopped or even it has never started
  if (thread_.joinable()) thread_.join();
}

void MysqlTaskMonitor::reset() {
  assert(state_.is(k_stopped));
  state_.set(k_initializing);
}

void MysqlTaskMonitor::run() {
  using namespace std::chrono_literals;

  mysql_thread_init();

  state_.exchange(k_initializing, k_running);

  my_thread_self_setname("Task monitor");

  log_system("Starting task monitor");

  while (!state_.is(k_stopped)) {
    {
      std::lock_guard<std::mutex> lock(tasks_mutex_);
      auto it = tasks_.begin();
      while (it != tasks_.end()) {
        auto next = it;
        ++next;
        try {
          if (update_task(*it)) {
            tasks_.erase(it);
          }
        } catch (const std::exception &e) {
          log_warning("Error executing async task: %s", e.what());
          tasks_.erase(it);
        }
        it = next;
      }
    }

    state_.wait_for(100ms, {k_stopped, k_check_tasks});
  }

  log_system("Stopping task monitor");

  mysql_thread_end();
}

void MysqlTaskMonitor::call_async(
    CachedSession session, PoolManagerRef session_pool,
    std::list<std::string> preamble, std::string script,
    std::list<std::string> postamble,
    std::function<std::list<std::string>(const std::exception &)> on_error,
    const std::string &task_id) {
  Task task{std::move(session_pool), std::move(session),
            std::move(preamble),     std::move(script),
            std::move(postamble),    {},
            std::move(on_error),     task_id};
  std::lock_guard<std::mutex> lock(tasks_mutex_);
  tasks_.emplace_back(std::move(task));
}

bool MysqlTaskMonitor::update_task(Task &task) {
  while (!task.preamble.empty()) {
    if (!task.session->execute_nb(task.preamble.front())) {
      // async not done yet
      return false;
    }
    task.preamble.pop_front();
  }

  if (!task.script.empty()) {
    try {
      if (!task.session->execute_nb(task.script)) {
        // async not done yet
        return false;
      }
      task.script.clear();
    } catch (const mysqlrouter::MySQLSession::Error &e) {
      task.script.clear();
      task.failed = true;
      // client errors are fatal (disconnection etc), so we can't cleanup
      if (e.code() >= CR_ERROR_FIRST && e.code() <= CR_ERROR_LAST) return true;
      task.error = task.on_error(e);
      return false;
    } catch (const std::exception &e) {
      task.script.clear();
      task.error = task.on_error(e);
      task.failed = true;
      return false;
    }
  }

  if (task.failed) {
    while (!task.error.empty()) {
      if (!task.session->execute_nb(task.error.front())) {
        // async not done yet
        return false;
      }
      task.error.pop_front();
    }
  } else {
    while (!task.postamble.empty()) {
      if (!task.session->execute_nb(task.postamble.front())) {
        // async not done yet
        return false;
      }
      task.postamble.pop_front();
    }
  }

  // all done
  return true;
}

}  // namespace database
}  // namespace mrs
