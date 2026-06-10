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

#include "mrs/database/slow_query_monitor.h"

#include <optional>
#include <utility>
#include <vector>
#include "helper/json/rapid_json_to_struct.h"
#include "helper/json/text_to.h"
#include "mrs/http/error.h"
#include "mrs/router_observation_entities.h"
#include "my_thread.h"  // NOLINT(build/include_subdir)
#include "mysql/harness/logging/logging.h"

IMPORT_LOG_FUNCTIONS()

namespace mrs {
namespace database {

namespace {
class SlowQueryOptions {
 public:
  std::optional<int64_t> sql_query_timeout;
};

class ParseSlowQueryOptions
    : public helper::json::RapidReaderHandlerToStruct<SlowQueryOptions> {
 public:
  template <typename ValueType>
  int64_t to_int(const ValueType &value) {
    return std::stoll(value.c_str());
  }

  template <typename ValueType>
  void handle_object_value(const std::string &key, const ValueType &vt) {
    if (key == "sqlQuery.timeout") {
      result_.sql_query_timeout = to_int(vt);
    }
  }

  template <typename ValueType>
  void handle_value(const ValueType &vt) {
    const auto &key = get_current_key();
    if (is_object_path()) {
      handle_object_value(key, vt);
    }
  }

  bool RawNumber(const Ch *v, rapidjson::SizeType v_len, bool) override {
    handle_value(std::string{v, v_len});
    return true;
  }
};

auto parse_slow_query_options(const std::string &options) {
  auto result = helper::json::text_to_handler<ParseSlowQueryOptions>(options);

  if (!result) {
    log_error(
        "Failed to parse 'SlowQueryMonitor' options from global JSON "
        "configuration");
  }

  return result.value_or(ParseSlowQueryOptions::Result{});
}

}  // namespace

SlowQueryMonitor::SlowQueryMonitor(const mrs::Configuration &configuration,
                                   collector::MysqlCacheManager *cache)
    : configuration_(configuration), cache_manager_(cache) {}

void SlowQueryMonitor::configure(const std::string &options) {
  auto opts = parse_slow_query_options(options);

  default_sql_timeout_ms_ =
      opts.sql_query_timeout.value_or(k_default_sql_query_timeout_ms);

  log_debug("SlowQueryMonitor::%s sqlQueryTimeout=%" PRId64, __FUNCTION__,
            default_sql_timeout_ms_);
}

void SlowQueryMonitor::execute(const std::function<void()> &fn,
                               MySQLSession *conn, int64_t timeout_ms) {
  auto state = on_query_start(conn, timeout_ms);
  try {
    fn();
  } catch (const mysqlrouter::MySQLSession::Error &e) {
    auto killed = state->killed;
    on_query_end(state);
    if (e.code() == 2013 && killed)
      throw http::Error(HttpStatusCode::GatewayTimeout,
                        "Database request timed out");
    else
      throw;
  } catch (...) {
    on_query_end(state);
    throw;
  }
  on_query_end(state);
}

SlowQueryMonitor::ActiveQueryListIt SlowQueryMonitor::on_query_start(
    MySQLSession *conn, int64_t timeout_ms) {
  log_debug("SlowQueryMonitor::%s (%" PRId64 ")", __FUNCTION__, timeout_ms);

  auto now = TimeType::clock::now();

  ActiveQuery query;
  query.conn = conn;
  query.connection_id = conn->connection_id();
  query.max_time =
      now + std::chrono::milliseconds(timeout_ms > 0 ? timeout_ms
                                                     : default_sql_timeout_ms_);

  bool needs_wakeup;
  ActiveQueryListIt ret;
  {
    std::lock_guard<std::mutex> lock(active_queries_mutex_);
    auto it = std::lower_bound(
        active_queries_.begin(), active_queries_.end(), query,
        [](const auto &a, const auto &b) { return a.max_time < b.max_time; });

    needs_wakeup = active_queries_.empty();
    ret = active_queries_.emplace(it, std::move(query));
  }

  if (needs_wakeup) {
    waitable_.serialize_with_cv([this](void *, std::condition_variable &cv) {
      if (state_.exchange({k_idle}, k_running)) {
        cv.notify_all();
      }
    });
  }

  return ret;
}

void SlowQueryMonitor::on_query_end(ActiveQueryListIt query) {
  log_debug("SlowQueryMonitor::%s", __FUNCTION__);

  std::lock_guard<std::mutex> lock(active_queries_mutex_);
  active_queries_.erase(query);
}

void SlowQueryMonitor::kill_session(
    const collector::CountedMySQLSession::ConnectionParameters &params,
    unsigned long conn_id) {
  std::string q = "KILL " + std::to_string(conn_id);

  log_info("SQL time out, killing %lu", conn_id);

  mrs::Counter<kEntityCounterSqlQueryTimeouts>::increment();

  try {
    auto session = cache_manager_->clone_instance(params);

    session->execute(q);
  } catch (const std::exception &e) {
    log_warning("Error killing connection at %s: %s",
                params.conn_opts.destination.str().c_str(), e.what());
  }
}

int64_t SlowQueryMonitor::check_queries() {
  log_debug("SlowQueryMonitor::%s", __FUNCTION__);

  std::vector<std::pair<collector::CountedMySQLSession::ConnectionParameters,
                        unsigned long>>
      kill_list;

  int64_t next_timeout = 0;
  {
    std::lock_guard<std::mutex> lock(active_queries_mutex_);
    auto now = TimeType::clock::now();

    for (auto &q : active_queries_) {
      if (q.max_time <= now) {
        if (!q.killed) {
          q.killed = true;
          kill_list.emplace_back(q.conn->get_connection_parameters(),
                                 q.connection_id);
        }
      } else {
        // +1 to prevent sleep forever when duration rounds down to 0
        next_timeout = std::chrono::duration_cast<std::chrono::milliseconds>(
                           q.max_time - now)
                           .count() +
                       1;
        break;
      }
    }
  }
  for (const auto &it : kill_list) kill_session(it.first, it.second);

  return next_timeout;
}

SlowQueryMonitor::~SlowQueryMonitor() { stop(); }

void SlowQueryMonitor::start() {
  log_debug("SlowQueryMonitor::%s", __FUNCTION__);
  monitor_thread_ = std::thread([this]() { run(); });

  state_.wait({k_idle});
}

void SlowQueryMonitor::stop() {
  waitable_.serialize_with_cv([this](void *, std::condition_variable &cv) {
    if (state_.exchange({k_initializing, k_running, k_idle}, k_stopped)) {
      log_debug("SlowQueryMonitor::%s", __FUNCTION__);
      cv.notify_all();
    }
  });
  // The thread might be already stopped or even it has never started
  if (monitor_thread_.joinable()) monitor_thread_.join();
}

void SlowQueryMonitor::reset() {
  assert(state_.is(k_stopped));
  state_.set(k_initializing);
}

void SlowQueryMonitor::run() {
  state_.exchange(k_initializing, k_idle);

  my_thread_self_setname("Slow query monitor");

  log_system("Starting slow query monitor");

  int64_t next_timeout = 0;

  do {
    next_timeout = check_queries();

    if (next_timeout <= 0) {
      state_.exchange(k_running, k_idle);

      next_timeout = 0;
    }

  } while (wait_until_next_timeout(next_timeout));

  log_system("Stopping slow query monitor");
}

bool SlowQueryMonitor::wait_until_next_timeout(int64_t next_timeout) {
  log_debug("%s (%" PRId64 ")", __FUNCTION__, next_timeout);

  if (next_timeout == 0) {
    waitable_.wait([this](void *) {
      return state_.is({k_running, k_stopped});
    });
  } else {
    waitable_.wait_for(std::chrono::milliseconds(next_timeout),
                       [this](void *) { return !state_.is(k_running); });
  }
  return state_.is({k_running, k_idle});
}

}  // namespace database
}  // namespace mrs
