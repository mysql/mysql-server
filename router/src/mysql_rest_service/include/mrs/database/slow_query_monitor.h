/*
  Copyright (c) 2024, 2026, Oracle and/or its affiliates.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2.0,
  as published by the Free Software Foundation.

  This program is designed to work with certain software (including
  but not limited to OpenSSL) that is licensed under separate terms,
  as designated in a particular file or component or in included license
  documentation.  The authors of MySQL hereby grant you an additional
  permission to link the program and your derivative works with the
  separately licensed software that they have either included with
  the program or referenced in the documentation.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef ROUTER_SRC_REST_MRS_SRC_MRS_SLOW_QUERY_MONITOR_H_
#define ROUTER_SRC_REST_MRS_SRC_MRS_SLOW_QUERY_MONITOR_H_

#include <chrono>
#include <functional>
#include <list>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include "collector/counted_mysql_session.h"
#include "collector/mysql_cache_manager.h"
#include "mrs/configuration.h"
#include "mysql/harness/stdx/monitor.h"
#include "mysql/harness/utility/wait_variable.h"

namespace mrs {
namespace database {

constexpr const int64_t k_default_sql_query_timeout_ms = 2000;

class SlowQueryMonitor {
 public:
  using MySQLSession = collector::CountedMySQLSession;

 public:
  SlowQueryMonitor(const mrs::Configuration &configuration,
                   collector::MysqlCacheManager *cache);
  ~SlowQueryMonitor();

  void execute(const std::function<void()> &fn, MySQLSession *conn,
               int64_t timeout_ms = -1);

  void start();
  void stop();
  void reset();

  void configure(const std::string &options);

  int64_t default_timeout() const { return default_sql_timeout_ms_; }

 private:
  using TimeType = std::chrono::time_point<std::chrono::system_clock>;
  template <typename T>
  using WaitableVariable = mysql_harness::utility::WaitableVariable<T>;

  struct ActiveQuery {
    MySQLSession *conn;
    bool killed = false;
    uint64_t connection_id = 0;

    TimeType max_time;
  };

  using ActiveQueryList = std::list<ActiveQuery>;
  using ActiveQueryListIt = ActiveQueryList::iterator;

  ActiveQueryListIt on_query_start(MySQLSession *conn, int64_t timeout_ms = -1);
  void on_query_end(ActiveQueryListIt query);

  void run();
  bool wait_until_next_timeout(int64_t next_timeout);

  void kill_session(
      const collector::CountedMySQLSession::ConnectionParameters &params,
      unsigned long conn_id);
  int64_t check_queries();

  uint64_t default_sql_timeout_ms_{2000};

  enum State { k_initializing, k_running, k_idle, k_stopped };

  std::thread monitor_thread_;
  const mrs::Configuration configuration_;
  collector::MysqlCacheManager *cache_manager_;
  WaitableVariable<State> state_{k_initializing};

  class Waitable : public WaitableMonitor<void *> {
   public:
    using Parent = WaitableMonitor<void *>;
    using Parent::WaitableMonitor;
  };
  Waitable waitable_{this};

  std::mutex active_queries_mutex_;
  ActiveQueryList active_queries_;
};

}  // namespace database
}  // namespace mrs

#endif  // ROUTER_SRC_REST_MRS_SRC_MRS_SLOW_QUERY_MONITOR_H_
