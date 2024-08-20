/*
  Copyright (c) 2022, 2024, Oracle and/or its affiliates.

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

#include "classic_lazy_connect.h"

#include <chrono>
#include <deque>
#include <initializer_list>
#include <memory>
#include <optional>
#include <ratio>
#include <sstream>

#include "classic_auth_caching_sha2.h"
#include "classic_change_user_sender.h"
#include "classic_connect.h"
#include "classic_connection_base.h"
#include "classic_frame.h"
#include "classic_greeting_forwarder.h"  // ServerGreetor
#include "classic_init_schema_sender.h"
#include "classic_query_sender.h"
#include "classic_reset_connection_sender.h"
#include "classic_set_option_sender.h"
#include "mysql/harness/logging/logging.h"
#include "mysql/harness/stdx/expected.h"
#include "mysql_com.h"
#include "mysqlrouter/classic_protocol_message.h"
#include "mysqlrouter/connection_pool_component.h"
#include "mysqlrouter/datatypes.h"
#include "mysqlrouter/utils.h"  // to_string
#include "router_require.h"
#include "sql_value.h"  // sql_value_to_string

IMPORT_LOG_FUNCTIONS()

using namespace std::chrono_literals;

namespace {

class FailedQueryHandler : public QuerySender::Handler {
 public:
  FailedQueryHandler(LazyConnector &processor, std::string stmt)
      : processor_(processor), stmt_(std::move(stmt)) {}

  void on_error(const classic_protocol::message::server::Error &err) override {
    log_warning("Executing %s failed: %s", stmt_.c_str(),
                err.message().c_str());

    processor_.failed(err);
  }

 private:
  LazyConnector &processor_;

  std::string stmt_;
};

class IsTrueHandler : public QuerySender::Handler {
 public:
  IsTrueHandler(LazyConnector &processor,
                classic_protocol::message::server::Error on_cond_fail_error)
      : processor_(processor),
        on_condition_fail_error_(std::move(on_cond_fail_error)) {}

  void on_column_count(uint64_t count) override {
    if (count != 1) {
      processor_.failed(classic_protocol::message::server::Error{
          0, "Too many columns", "HY000"});
    }
  }

  void on_row(const classic_protocol::message::server::Row &row) override {
    ++row_count_;

    if (row.begin() == row.end()) {
      processor_.failed(
          classic_protocol::message::server::Error{0, "No fields", "HY000"});
      return;
    }

    auto fld = *(row.begin());

    if (!fld.has_value()) {
      processor_.failed(classic_protocol::message::server::Error{
          0, "Expected integer, got NULL", "HY000"});
      return;
    }

    if (*fld != "1") {
      processor_.failed(on_condition_fail_error_);
      return;
    }
  }

  void on_row_end(
      const classic_protocol::message::server::Eof & /* eof */) override {
    if (row_count_ != 1) {
      processor_.failed(classic_protocol::message::server::Error{
          0, "Too many rows", "HY000"});
      return;
    }
  }

  void on_error(const classic_protocol::message::server::Error &err) override {
    log_warning("%s", err.message().c_str());

    processor_.failed(err);
  }

 private:
  LazyConnector &processor_;
  uint64_t row_count_{};

  classic_protocol::message::server::Error on_condition_fail_error_;
};

/**
 * capture the system-variables.
 *
 * Expects a resultset similar to that of:
 *
 * @code
 * SELECT <key>, <value>
 *   FROM performance_schema.session_variables
 *  WHERE VARIABLE_NAME IN ('collation_connection')
 * @endcode
 *
 * - 2 columns (column-names are ignored)
 * - multiple rows
 */
class SelectSessionVariablesHandler : public QuerySender::Handler {
 public:
  SelectSessionVariablesHandler(MysqlRoutingClassicConnectionBase *connection)
      : connection_(connection) {}

  void on_column_count(uint64_t count) override {
    col_count_ = count;

    if (col_count_ != 2) {
      something_failed_ = true;
    }
  }

  void on_column(const classic_protocol::message::server::ColumnMeta
                     & /* col */) override {
    if (something_failed_) return;
  }

  void on_row(const classic_protocol::message::server::Row &row) override {
    if (something_failed_) return;

    auto it = row.begin();  // row[0]

    if (!(*it).has_value()) {
      something_failed_ = true;
      return;
    }

    std::string key = it->value();

    ++it;  // row[1]

    session_variables_.emplace_back(key, *it);
  }

  void on_row_end(
      const classic_protocol::message::server::Eof & /* eof */) override {
    if (something_failed_) {
      // something failed when parsing the resultset. Disable sharing for now.
      connection_->some_state_changed(true);
    } else {
      // move all captured session-vars to the system-variable storage.
      for (; !session_variables_.empty(); session_variables_.pop_front()) {
        auto &node = session_variables_.front();

        connection_->client_protocol().system_variables().set(node.first,
                                                              node.second);
        connection_->server_protocol().system_variables().set(node.first,
                                                              node.second);
      }
    }
  }

  void on_ok(const classic_protocol::message::server::Ok & /* ok */) override {
    // ok, shouldn't happen. Disable sharing for now.
    connection_->some_state_changed(true);
  }

  void on_error(const classic_protocol::message::server::Error &err) override {
    // error, shouldn't happen. Disable sharing for now.
    log_debug("Fetching system-vars failed: %s", err.message().c_str());

    connection_->some_state_changed(true);
  }

 private:
  uint64_t col_count_{};
  uint64_t col_cur_{};
  MysqlRoutingClassicConnectionBase *connection_;

  bool something_failed_{false};

  std::deque<std::pair<std::string, std::optional<std::string>>>
      session_variables_;
};

}  // namespace

stdx::expected<Processor::Result, std::error_code> LazyConnector::process() {
  switch (stage()) {
    case Stage::Init:
      return init();
    case Stage::FromStash:
      return from_stash();
    case Stage::Connect:
      return connect();
    case Stage::Connected:
      return connected();
    case Stage::Authenticated:
      return authenticated();
    case Stage::FetchUserAttrs:
      return fetch_user_attrs();
    case Stage::FetchUserAttrsDone:
      return fetch_user_attrs_done();
    case Stage::SendAuthOk:
      return send_auth_ok();
    case Stage::SetSchema:
      return set_schema();
    case Stage::SetSchemaDone:
      return set_schema_done();
    case Stage::SetServerOption:
      return set_server_option();
    case Stage::SetServerOptionDone:
      return set_server_option_done();
    case Stage::SetVars:
      return set_vars();
    case Stage::SetVarsDone:
      return set_vars_done();
    case Stage::FetchSysVars:
      return fetch_sys_vars();
    case Stage::FetchSysVarsDone:
      return fetch_sys_vars_done();
    case Stage::SetTrxCharacteristics:
      return set_trx_characteristics();
    case Stage::SetTrxCharacteristicsDone:
      return set_trx_characteristics_done();
    case Stage::WaitGtidExecuted:
      return wait_gtid_executed();
    case Stage::WaitGtidExecutedDone:
      return wait_gtid_executed_done();
    case Stage::PoolOrClose:
      return pool_or_close();
    case Stage::FallbackToWrite:
      return fallback_to_write();
    case Stage::Done:

      if (failed()) {
        if (auto &tr = tracer()) {
          tr.trace(Tracer::Event().stage("connect::failed"));
        }

        if (on_error_) on_error_(*failed());
        connection()->authenticated(false);
      }

      // reset the seq-id of the server side as this is a new command.
      connection()->server_protocol().seq_id(0xff);

      trace_span_end(trace_event_connect_);

      return Result::Done;
  }

  harness_assert_this_should_not_execute();
}

stdx::expected<Processor::Result, std::error_code> LazyConnector::init() {
  connection()->current_server_mode(connection()->expected_server_mode());

  stage(Stage::FromStash);
  return Result::Again;
}

stdx::expected<Processor::Result, std::error_code> LazyConnector::from_stash() {
  connection()->has_transient_error_at_connect(false);

  trace_event_connect_ =
      trace_span(parent_event_, "mysql/prepare_server_connection");

  if (!connection()->server_conn().is_open()) {
    if (auto ep = connection()->destination_endpoint()) {
      if (auto &tr = tracer()) {
        tr.trace(Tracer::Event().stage("connect::from_stash"));
      }

      auto &pool_comp = ConnectionPoolComponent::get_instance();

      if (auto pool =
              pool_comp.get(ConnectionPoolComponent::default_pool_name())) {
        TraceEvent *trace_event_from_stash = nullptr;

        if (auto *ev = trace_event_connect_) {
          trace_event_from_stash = trace_span(ev, "mysql/from_stash");
        }

        if (auto pop_res =
                pool->unstash_mine(mysqlrouter::to_string(*ep), connection())) {
          connection()->server_conn() = std::move(*pop_res);

          // reset the seq-id of the server side as this is a new command.
          connection()->server_protocol().seq_id(0xff);

          if (auto &tr = tracer()) {
            tr.trace(Tracer::Event().stage(
                "connect::from_stash::unstashed::mine: fd=" +
                std::to_string(connection()->server_conn().native_handle()) +
                ", " + connection()->server_conn().endpoint()));
          }

          if (auto *ev = trace_event_from_stash) {
            trace_set_connection_attributes(ev);
            trace_span_end(ev);
          }

          stage(Stage::SetVars);
          return Result::Again;
        }

        if (auto *ev = trace_event_from_stash) {
          ev->attrs.emplace_back("mysql.error_message", "no match");
          trace_span_end(ev, TraceEvent::StatusCode::kError);
        }
      }
    }
  }

  stage(Stage::Connect);

  return Result::Again;
}

stdx::expected<Processor::Result, std::error_code> LazyConnector::connect() {
  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("connect::connect"));
  }

  auto &server_conn = connection()->server_conn();

  if (!server_conn.is_open()) {
    stage(Stage::Connected);

    // creates a fresh connection or takes one from the pool.
    connection()->push_processor(std::make_unique<ConnectProcessor>(
        connection(),
        [this](const classic_protocol::message::server::Error &err) {
          on_error_(err);
        },
        trace_event_connect_));
  } else {
    stage(Stage::Done);  // there still is a connection open, nothing to do.
  }

  return Result::Again;
}

/**
 * the handshake part.
 */
stdx::expected<Processor::Result, std::error_code> LazyConnector::connected() {
  auto &server_conn = connection()->server_conn();
  auto &client_protocol = connection()->client_conn().protocol();
  auto &server_protocol = server_conn.protocol();

  if (!server_conn.is_open()) {
    if (auto &tr = tracer()) {
      tr.trace(Tracer::Event().stage("connect::not_connected"));
    }

    // looks like connection failed, leave.
    stage(Stage::Done);
    return Result::Again;
  }

  trace_event_authenticate_ =
      trace_span(trace_event_connect_, "mysql/authenticate");

  // remember the trx-stmt as it will be overwritten by set_session_vars.
  if (connection()->trx_characteristics()) {
    trx_stmt_ = connection()->trx_characteristics()->characteristics();
  }

  /*
   * if the connection is from the pool, we need a change user.
   */
  if (server_protocol.server_greeting()) {
    connection()->server_conn().protocol().handshake_state(
        ClassicProtocolState::HandshakeState::kFinished);

    if (!in_handshake_ &&
        ((client_protocol.username() == server_protocol.username()) &&
         (client_protocol.sent_attributes() ==
          server_protocol.sent_attributes()))) {
      // it is ok if the schema differs, it will be handled later set_schema()

      if (auto *ev = trace_event_authenticate_) {
        ev->attrs.emplace_back("mysql.remote.needs_full_handshake", false);
      }

      connection()->push_processor(std::make_unique<ResetConnectionSender>(
          connection(), trace_event_authenticate_));
      connection()->authenticated(true);
    } else {
      if (auto *ev = trace_event_authenticate_) {
        ev->attrs.emplace_back("mysql.remote.needs_full_handshake", true);
        ev->attrs.emplace_back(
            "mysql.remote.username_differs",
            client_protocol.username() == server_protocol.username());
        ev->attrs.emplace_back("mysql.remote.connection_attributes_differ",
                               client_protocol.sent_attributes() ==
                                   server_protocol.sent_attributes());
      }

      connection()->push_processor(std::make_unique<ChangeUserSender>(
          connection(), in_handshake_,
          [this](const classic_protocol::message::server::Error &err) {
            on_error_(err);
          },
          trace_event_authenticate_));
    }
  } else {
    if (auto *ev = trace_event_authenticate_) {
      ev->attrs.emplace_back("mysql.remote.needs_full_handshake", true);
    }

    connection()->push_processor(std::make_unique<ServerGreetor>(
        connection(), in_handshake_,
        [this](const classic_protocol::message::server::Error &err) {
          if (connect_error_is_transient(err) &&
              (connection()->client_protocol().credentials().get(
                   AuthCachingSha2Password::kName) ||
               !connection()
                    ->server_protocol()
                    .server_greeting()
                    .has_value()) &&
              std::chrono::steady_clock::now() <
                  started_ + connection()->context().connect_retry_timeout()) {
            // the error is transient.
            //
            // try to reconnect as long as the connect-timeout hasn't been
            // reached yet.

            // only try to reconnect, if
            //
            // 1. the connect failed in the server-greeting
            // 2. the client's password is known as otherwise client would
            // receive the auth-switch several times as part of the auth
            // handshake.

            retry_connect_ = true;
          } else {
            // propagate the error up to the caller.
            on_error_(err);
          }
        },
        trace_event_authenticate_));
  }

  stage(Stage::Authenticated);
  return Result::Again;
}

stdx::expected<Processor::Result, std::error_code>
LazyConnector::authenticated() {
  if (!connection()->authenticated() ||
      !connection()->server_conn().is_open()) {
    if (auto &tr = tracer()) {
      tr.trace(Tracer::Event().stage("connect::authenticate::error"));
    }

    if (auto *ev = trace_event_authenticate_) {
      trace_span_end(ev, TraceEvent::StatusCode::kError);
    }

    if (retry_connect_) {
      retry_connect_ = false;

      connection()->has_transient_error_at_connect(true);

      stage(Stage::Connect);
      connection()->connect_timer().expires_after(kConnectRetryInterval);
      connection()->connect_timer().async_wait([this](std::error_code ec) {
        if (ec) return;

        connection()->resume();
      });

      return Result::Suspend;
    }

    stage(Stage::Done);
    return Result::Again;
  }

  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("connect::authenticate::ok"));
  }

  if (auto *ev = trace_event_authenticate_) {
    trace_span_end(ev);
  }

  stage(Stage::SetVars);
  return Result::Again;
}

namespace {
void set_session_var(std::string &q, const std::string &key,
                     const std::optional<std::string> &val) {
  if (q.empty()) {
    q = "SET ";
  } else {
    q += ",\n    ";
  }

  q += "@@SESSION." + key + " = " + sql_value_to_string(val);
}

void set_session_var_or_value(
    std::string &q, const ClassicProtocolState::SystemVariables &sysvars,
    const std::string &key, const std::optional<std::string> &default_value) {
  auto value = sysvars.get(key);
  if (value == std::nullopt) {
    set_session_var(q, key, default_value);
  } else {
    set_session_var(q, key, value);
  }
}
}  // namespace

stdx::expected<Processor::Result, std::error_code> LazyConnector::set_vars() {
  auto &client_sysvars = connection()->client_protocol().system_variables();
  auto &server_sysvars = connection()->server_protocol().system_variables();

  std::string stmt;

  const auto need_session_trackers =
      connection()->context().connection_sharing() &&
      connection()->greeting_from_router();

  // must be first, to track all variables that are set.
  if (need_session_trackers) {
    auto server_sysvar_has =
        server_sysvars.find("session_track_system_variables");

    if (!server_sysvar_has || *server_sysvar_has != "*") {
      set_session_var_or_value(stmt, client_sysvars,
                               "session_track_system_variables", "*");
    }
  } else {
    auto var = client_sysvars.get("session_track_system_variables");
    if (var != std::nullopt) {
      set_session_var(stmt, "session_track_system_variables", var);
    }
  }

  for (const auto &var : client_sysvars) {
    // already set earlier.
    if (var.first == "session_track_system_variables") continue;

    // is read-only
    if (var.first == "statement_id") continue;
    auto server_sysvar_has = server_sysvars.find(var.first);

    if (!server_sysvar_has || *server_sysvar_has != var.second) {
      set_session_var(stmt, var.first, var.second);
    }
  }

  if (need_session_trackers) {
    for (auto [k, v] : std::initializer_list<
             std::pair<std::string, std::optional<std::string>>>{
             {"session_track_gtids", "OWN_GTID"},
             {"session_track_schema", "ON"},
             {"session_track_state_change", "ON"},
             {"session_track_transaction_info", "CHARACTERISTICS"},
         }) {
      auto client_sysvar_has = client_sysvars.find(k);
      if (client_sysvar_has) continue;  // already handled above.

      auto server_sysvar_has = server_sysvars.find(k);

      if (!server_sysvar_has || *server_sysvar_has != v) {
        set_session_var(stmt, k, v);
      }
    }
  }

  if (!stmt.empty()) {
    stage(Stage::SetVarsDone);

    if (auto &tr = tracer()) {
      tr.trace(Tracer::Event().stage("connect::set_var"));
    }

    trace_event_set_vars_ = trace_span(trace_event_connect_, "mysql/set_var");
    if (auto *ev = trace_event_set_vars_) {
      for (const auto &var : client_sysvars) {
        if (var.first == "statement_id") continue;

        if (var.second) {
          ev->attrs.emplace_back(
              "mysql.session.@@SESSION." + var.first,
              TraceEvent::element_type::second_type{*var.second});
        } else {
          // NULL
          ev->attrs.emplace_back(var.first,
                                 TraceEvent::element_type::second_type{});
        }
      }
    }

    connection()->push_processor(std::make_unique<QuerySender>(
        connection(), stmt, std::make_unique<FailedQueryHandler>(*this, stmt)));
  } else {
    stage(Stage::SetServerOption);
  }
  return Result::Again;
}

stdx::expected<Processor::Result, std::error_code>
LazyConnector::set_vars_done() {
  if (auto *ev = trace_event_set_vars_) {
    trace_span_end(ev);
  }

  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("connect::set_var::done"));
  }

  stage(Stage::SetServerOption);
  return Result::Again;
}

stdx::expected<Processor::Result, std::error_code>
LazyConnector::set_server_option() {
  auto &src_protocol = connection()->client_protocol();
  auto &dst_protocol = connection()->server_protocol();

  bool client_has_multi_statements = src_protocol.client_capabilities().test(
      classic_protocol::capabilities::pos::multi_statements);
  bool server_has_multi_statements = dst_protocol.client_capabilities().test(
      classic_protocol::capabilities::pos::multi_statements);

  if (client_has_multi_statements == server_has_multi_statements) {
    stage(Stage::FetchSysVars);
    return Result::Again;
  }

  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("connect::set_server_option"));
  }

  stage(Stage::SetServerOptionDone);
  connection()->push_processor(std::make_unique<SetOptionSender>(
      connection(), client_has_multi_statements
                        ? MYSQL_OPTION_MULTI_STATEMENTS_ON
                        : MYSQL_OPTION_MULTI_STATEMENTS_OFF));

  return Result::Again;
}

stdx::expected<Processor::Result, std::error_code>
LazyConnector::set_server_option_done() {
  if (failed()) {
    if (auto &tr = tracer()) {
      tr.trace(Tracer::Event().stage("connect::set_server_option::failed"));
    }
    stage(Stage::Done);
  } else {
    if (auto &tr = tracer()) {
      tr.trace(Tracer::Event().stage("connect::set_server_option::done"));
    }

    stage(Stage::FetchSysVars);
  }

  return Result::Again;
}

stdx::expected<Processor::Result, std::error_code>
LazyConnector::fetch_sys_vars() {
  std::ostringstream oss;

  if (connection()->connection_sharing_possible()) {
    // fetch the sys-vars that aren't known yet.
    for (const auto &expected_var :
         {"collation_connection", "character_set_client", "sql_mode"}) {
      const auto &sys_vars = connection()->client_protocol().system_variables();
      auto find_res = sys_vars.find(expected_var);
      if (!find_res) {
        if (oss.tellp() != 0) {
          oss << " UNION ";
        }

        // use ' to quote to make it ANSI_QUOTES safe.
        oss << "SELECT " << std::quoted(expected_var, '\'') << ", @@SESSION."
            << std::quoted(expected_var, '`');
      }
    }
  }

  if (oss.tellp() != 0) {
    trace_event_fetch_sys_vars_ =
        trace_span(trace_event_connect_, "mysql/fetch_sys_vars");

    if (auto &tr = tracer()) {
      tr.trace(Tracer::Event().stage("connect::fetch_sys_vars"));
    }

    stage(Stage::FetchSysVarsDone);

    connection()->push_processor(std::make_unique<QuerySender>(
        connection(), oss.str(),
        std::make_unique<SelectSessionVariablesHandler>(connection())));
  } else {
    stage(Stage::SetSchema);
  }

  return Result::Again;
}

stdx::expected<Processor::Result, std::error_code>
LazyConnector::fetch_sys_vars_done() {
  trace_span_end(trace_event_fetch_sys_vars_);

  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("connect::fetch_sys_vars::done"));
  }

  stage(Stage::SetSchema);
  return Result::Again;
}

stdx::expected<Processor::Result, std::error_code> LazyConnector::set_schema() {
  auto client_schema = connection()->client_conn().protocol().schema();
  auto server_schema = connection()->server_conn().protocol().schema();

  if (!client_schema.empty() && (client_schema != server_schema)) {
    if (auto &tr = tracer()) {
      tr.trace(Tracer::Event().stage("connect::set_schema"));
    }

    trace_event_set_schema_ =
        trace_span(trace_event_connect_, "mysql/set_schema");

    stage(Stage::SetSchemaDone);

    connection()->push_processor(
        std::make_unique<InitSchemaSender>(connection(), client_schema));
  } else {
    stage(Stage::WaitGtidExecuted);  // skip set_schema_done
  }

  return Result::Again;
}

stdx::expected<Processor::Result, std::error_code>
LazyConnector::set_schema_done() {
  if (auto *ev = trace_event_set_schema_) {
    trace_span_end(ev);
  }

  if (failed()) {
    if (auto &tr = tracer()) {
      tr.trace(Tracer::Event().stage("connect::set_schema::failed"));
    }

    stage(Stage::Done);
    return Result::Again;
  }

  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("connect::set_schema::done"));
  }

  stage(Stage::WaitGtidExecuted);

  return Result::Again;
}

stdx::expected<Processor::Result, std::error_code>
LazyConnector::wait_gtid_executed() {
  stage(Stage::SetTrxCharacteristics);  // skip wait_gtid_executed_done if we
                                        // didn't wait.

  if (connection()->wait_for_my_writes() &&
      (connection()->current_server_mode() ==
       mysqlrouter::ServerMode::ReadOnly)) {
    auto gtid_executed = connection()->gtid_at_least_executed();
    if (!gtid_executed.empty()) {
      if (auto &tr = tracer()) {
        tr.trace(Tracer::Event().stage("connect::wait_gtid"));
      }

      trace_event_wait_gtid_executed_ =
          trace_span(trace_event_connect_, "mysql/wait_gtid_executed");

      stage(Stage::WaitGtidExecutedDone);

      const std::chrono::seconds max_replication_lag{
          connection()->wait_for_my_writes_timeout()};

      std::ostringstream oss;
      if (max_replication_lag.count() == 0) {
        oss << "SELECT GTID_SUBSET(" << std::quoted(gtid_executed)
            << ", @@GLOBAL.gtid_executed)";
      } else {
        oss << "SELECT NOT WAIT_FOR_EXECUTED_GTID_SET("
            << std::quoted(gtid_executed) << ", "
            << std::to_string(max_replication_lag.count()) << ")";
      }

      connection()->push_processor(std::make_unique<QuerySender>(
          connection(), oss.str(),
          std::make_unique<IsTrueHandler>(
              *this, classic_protocol::message::server::Error{
                         0, "wait_for_my_writes timed out", "HY000"})));
    }
  }

  return Result::Again;
}

stdx::expected<Processor::Result, std::error_code>
LazyConnector::wait_gtid_executed_done() {
  if (failed()) {
    if (auto &tr = tracer()) {
      tr.trace(Tracer::Event().stage("connect::wait_gtid::failed"));
    }

    trace_span_end(trace_event_wait_gtid_executed_,
                   TraceEvent::StatusCode::kError);

    stage(Stage::PoolOrClose);
  } else {
    if (auto &tr = tracer()) {
      tr.trace(Tracer::Event().stage("connect::wait_gtid::done"));
    }

    trace_span_end(trace_event_wait_gtid_executed_);

    stage(Stage::SetTrxCharacteristics);
  }
  return Result::Again;
}

stdx::expected<Processor::Result, std::error_code>
LazyConnector::pool_or_close() {
  stage(Stage::FallbackToWrite);

  connection()->stash_server_conn();

  return Result::Again;
}

stdx::expected<Processor::Result, std::error_code>
LazyConnector::fallback_to_write() {
  if (already_fallback_ ||
      (connection()->expected_server_mode() ==
       mysqlrouter::ServerMode::ReadWrite) ||
      (connection()->current_server_mode() ==
       mysqlrouter::ServerMode::ReadWrite)) {
    // only fallback to the primary once and if the client is asking for
    // "read-only" nodes

    // failed() is already set.

    stage(Stage::Done);
    return Result::Again;
  }

  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("connect::fallback_to_write"));
  }

  // connect to the read-write node in read-only mode.
  connection()->current_server_mode(mysqlrouter::ServerMode::ReadWrite);
  already_fallback_ = true;

  // reset the failed state
  failed(std::nullopt);

  // the fallback will create a new trace-event
  trace_span_end(trace_event_connect_);

  stage(Stage::FromStash);
  return Result::Again;
}

// restore the transaction characteristics as provided by the server's
// session-tracker.
//
// - zero-or-one isolation-level statement +
//   zero-or-one transaction state/start statement
// - seperated by semi-colon.
//
// - SET TRANSACTION ISOLATION LEVEL [...|SERIALIZABLE];
//
// - SET TRANSACTION READ ONLY;
// - START TRANSACTION [READ ONLY|READ WRITE], WITH CONSISTENT SNAPSHOT;
// - XA BEGIN;
//
stdx::expected<Processor::Result, std::error_code>
LazyConnector::set_trx_characteristics() {
  if (trx_stmt_.empty()) {
    stage(Stage::FetchUserAttrs);  // skip set_trx_characteristics_done
    return Result::Again;
  }

  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("connect::trx_characteristics"));
  }

  trace_event_set_trx_characteristics_ =
      trace_span(trace_event_connect_, "mysql/set_trx_characetristcs");

  stage(Stage::SetTrxCharacteristicsDone);

  // split the trx setup statements at the semi-colon
  auto trx_stmt = trx_stmt_;

  auto semi_pos = trx_stmt_.find(';');
  if (semi_pos == std::string::npos) {
    trx_stmt_.clear();
  } else {
    trx_stmt_.erase(0, semi_pos + 1);  // incl the semi-colon

    // if there is a leading space after the semi-colon, remove it too.
    if (!trx_stmt_.empty() && trx_stmt_[0] == ' ') {
      trx_stmt_.erase(0, 1);
    }

    trx_stmt.resize(semi_pos);
  }

  connection()->push_processor(std::make_unique<QuerySender>(
      connection(), trx_stmt,
      std::make_unique<FailedQueryHandler>(*this, trx_stmt)));

  return Result::Again;
}

stdx::expected<Processor::Result, std::error_code>
LazyConnector::set_trx_characteristics_done() {
  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("connect::trx_characteristics::done"));
  }

  if (failed()) {
    trace_span_end(trace_event_set_trx_characteristics_,
                   TraceEvent::StatusCode::kError);
  } else {
    trace_span_end(trace_event_set_trx_characteristics_);
  }

  // if there is more, execute the next part.
  stage(trx_stmt_.empty() ? Stage::FetchUserAttrs
                          : Stage::SetTrxCharacteristics);

  return Result::Again;
}

stdx::expected<Processor::Result, std::error_code>
LazyConnector::fetch_user_attrs() {
  if (!connection()->context().router_require_enforce()) {
    // skip the fetch-user-attrs.
    stage(Stage::SendAuthOk);
    return Result::Again;
  }

  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("connect::fetch_user_attrs"));
  }

  RouterRequireFetcher::push_processor(
      connection(), required_connection_attributes_fetcher_result_);

  stage(Stage::FetchUserAttrsDone);
  return Result::Again;
}

stdx::expected<Processor::Result, std::error_code>
LazyConnector::fetch_user_attrs_done() {
  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("connect::fetch_user_attrs::done"));
  }

  if (!required_connection_attributes_fetcher_result_) {
    failed(classic_protocol::message::server::Error{1045, "Access denied",
                                                    "28000"});

    stage(Stage::Done);
    return Result::Again;
  }

  auto enforce_res =
      RouterRequire::enforce(connection()->client_conn().channel(),
                             *required_connection_attributes_fetcher_result_);
  if (!enforce_res) {
    failed(classic_protocol::message::server::Error{1045, "Access denied",
                                                    "28000"});
    stage(Stage::Done);
    return Result::Again;
  }

  stage(Stage::SendAuthOk);
  return Result::Again;
}

stdx::expected<Processor::Result, std::error_code>
LazyConnector::send_auth_ok() {
  auto &dst_conn = connection()->client_conn();
  auto &dst_channel = dst_conn.channel();
  auto &dst_protocol = dst_conn.protocol();

  if (!in_handshake_) {
    stage(Stage::Done);
    return Result::Again;
  }

  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("connect::ok"));
  }

  // tell the client that everything is ok.
  auto send_res =
      ClassicFrame::send_msg<classic_protocol::borrowed::message::server::Ok>(
          dst_channel, dst_protocol, {0, 0, dst_protocol.status_flags(), 0});
  if (!send_res) return stdx::unexpected(send_res.error());

  stage(Stage::Done);
  return Result::SendToClient;
}
