/*
  Copyright (c) 2023, 2024, Oracle and/or its affiliates.

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

#include "classic_reset_connection_forwarder.h"

#include "classic_connection_base.h"
#include "classic_frame.h"
#include "classic_lazy_connect.h"
#include "classic_query_sender.h"
#include "mysql/harness/logging/logging.h"
#include "mysql/harness/net_ts/impl/poll.h"
#include "mysql/harness/stdx/expected.h"
#include "mysql/harness/tls_error.h"
#include "mysqld_error.h"  // mysql-server error-codes
#include "mysqlrouter/classic_protocol_session_track.h"
#include "mysqlrouter/connection_pool_component.h"
#include "mysqlrouter/datatypes.h"
#include "mysqlrouter/utils.h"  // to_string

IMPORT_LOG_FUNCTIONS()

namespace {

stdx::expected<void, std::error_code> socket_is_alive(
    const ConnectionPool::ServerSideConnection &server_conn) {
  std::array<net::impl::poll::poll_fd, 1> fds{
      {{server_conn.connection()->native_handle(), POLLIN, 0}}};
  auto poll_res = net::impl::poll::poll(fds.data(), fds.size(),
                                        std::chrono::milliseconds(0));
  if (!poll_res) {
    if (poll_res.error() != std::errc::timed_out) {
      // shouldn't happen, but if it does, ignore the socket.
      return stdx::unexpected(poll_res.error());
    }

    return {};
  }

  // there is data -> Error packet -> server closed the connection.
  return stdx::unexpected(make_error_code(net::stream_errc::eof));
}

class FailedQueryHandler : public QuerySender::Handler {
 public:
  FailedQueryHandler(ResetConnectionForwarder &processor, std::string stmt)
      : processor_(processor), stmt_(std::move(stmt)) {}

  void on_error(const classic_protocol::message::server::Error &err) override {
    log_warning("Executing %s failed: %s", stmt_.c_str(),
                err.message().c_str());

    processor_.failed(err);
  }

 private:
  ResetConnectionForwarder &processor_;

  std::string stmt_;
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

        connection_->execution_context().system_variables().set(
            std::move(node.first), std::move(node.second));
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

  std::deque<std::pair<std::string, Value>> session_variables_;
};

}  // namespace

/**
 * forward the reset-connection message flow.
 *
 * Expected overall flow:
 *
 * @code
 * c->s: COM_RESET_CONNECTION
 * c<-s: Ok
 * @endcode
 *
 * If there is no server connection, it is created on demand.
 */
stdx::expected<Processor::Result, std::error_code>
ResetConnectionForwarder::process() {
  switch (stage()) {
    case Stage::Command:
      return command();
    case Stage::StartLoop:
      return start_loop();
    case Stage::Connect:
      return connect();
    case Stage::Connected:
      return connected();
    case Stage::Response:
      return response();
    case Stage::Ok:
      return ok();
    case Stage::SetVars:
      return set_vars();
    case Stage::SetVarsDone:
      return set_vars_done();
    case Stage::FetchSysVars:
      return fetch_sys_vars();
    case Stage::FetchSysVarsDone:
      return fetch_sys_vars_done();
    case Stage::EndLoop:
      return end_loop();
    case Stage::SendOk:
      return send_ok();
    case Stage::Done:
      return Result::Done;
  }

  harness_assert_this_should_not_execute();
}

stdx::expected<Processor::Result, std::error_code>
ResetConnectionForwarder::command() {
  auto &src_conn = connection()->client_conn();

  auto msg_res = ClassicFrame::recv_msg<
      classic_protocol::borrowed::message::client::ResetConnection>(src_conn);
  if (!msg_res) {
    // all codec-errors should result in a Malformed Packet error..
    if (msg_res.error().category() !=
        make_error_code(classic_protocol::codec_errc::not_enough_input)
            .category()) {
      return recv_client_failed(msg_res.error());
    }

    discard_current_msg(src_conn);

    auto send_msg = ClassicFrame::send_msg<
        classic_protocol::borrowed::message::server::Error>(
        src_conn,
        {ER_MALFORMED_PACKET, "Malformed communication packet", "HY000"});
    if (!send_msg) send_client_failed(send_msg.error());

    stage(Stage::Done);

    return Result::SendToClient;
  }

  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("reset_connection::command"));
  }

  discard_current_msg(src_conn);

  // reset the session-state
  connection()->reset_to_initial();

  // reset-connection MUST reset *all* connections which are related to the
  // client-connection to release all temp-tables, locks, ...
  //
  // after the reset-connection the server-side connection MUST be prepared for
  // connection-sharing again (enable session-trackers, ...)
  //

  stage(Stage::StartLoop);

  return Result::Again;
}

stdx::expected<Processor::Result, std::error_code>
ResetConnectionForwarder::start_loop() {
  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("reset_connection::start_loop"));
  }

  // if there is a connection open or on the stash:
  //
  // - send it a reset-connection
  // - prepare it for connection-sharing.
  //
  // ... if not, open a new connection and let that reconnect
  // handle "prepare for connection-sharing".

  if (!connection()->server_conn().is_open() &&  //
      connection()->context().connection_sharing() &&
      connection()->greeting_from_router()) {
    // if there is no server-connection, perhaps there is one on the stash?
    auto &pool_comp = ConnectionPoolComponent::get_instance();

    if (auto pool =
            pool_comp.get(ConnectionPoolComponent::default_pool_name())) {
      if (auto ep = connection()->destination_endpoint()) {
        if (auto conn_res =
                pool->unstash_mine(mysqlrouter::to_string(*ep), connection())) {
          if (socket_is_alive(*conn_res)) {
            connection()->server_conn() = std::move(*conn_res);

            // reset the seq-id of the server side as this is a new command.
            connection()->server_protocol().seq_id(0xff);

            if (auto &tr = tracer()) {
              tr.trace(Tracer::Event().stage(
                  "reset_connection::from_stash::unstashed::mine: fd=" +
                  std::to_string(connection()->server_conn().native_handle()) +
                  ", " + connection()->server_conn().endpoint()));
            }
          }
        }
      }
    }
  }

  auto &server_conn = connection()->server_conn();
  if (server_conn.is_open()) {
    stage(Stage::Response);

    auto send_res = ClassicFrame::send_msg<
        classic_protocol::borrowed::message::client::ResetConnection>(
        server_conn, {});

    if (!send_res) return stdx::unexpected(send_res.error());

    return Result::SendToServer;
  }

  stage(Stage::Connect);
  return Result::Again;
}

stdx::expected<Processor::Result, std::error_code>
ResetConnectionForwarder::connect() {
  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("reset_connection::connect"));
  }

  stage(Stage::Connected);
  return mysql_reconnect_start(nullptr);
}

stdx::expected<Processor::Result, std::error_code>
ResetConnectionForwarder::connected() {
  auto &server_conn = connection()->server_conn();
  if (!server_conn.is_open()) {
    auto &src_conn = connection()->client_conn();

    if (auto &tr = tracer()) {
      tr.trace(Tracer::Event().stage("reset_connection::connect::error"));
    }

    stage(Stage::Done);
    return reconnect_send_error_msg(src_conn);
  }

  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("reset_connection::connected"));
  }

  // the reconnect returns a properly reset connection which requires not
  // set-vars from our side. Go to EndLoop directly.

  stage(Stage::EndLoop);
  return Result::Again;
}

stdx::expected<Processor::Result, std::error_code>
ResetConnectionForwarder::response() {
  auto &src_conn = connection()->server_conn();
  auto &src_protocol = src_conn.protocol();

  auto read_res = ClassicFrame::ensure_has_msg_prefix(src_conn);
  if (!read_res) return recv_server_failed(read_res.error());

  const uint8_t msg_type = src_protocol.current_msg_type().value();

  enum class Msg {
    Ok = ClassicFrame::cmd_byte<classic_protocol::message::server::Ok>(),
  };

  // reset-connection is not expected to fail.
  switch (Msg{msg_type}) {
    case Msg::Ok:
      stage(Stage::Ok);
      return Result::Again;
  }

  log_debug("reset_connection::response: unexpected msg-type '%02x'", msg_type);

  return stdx::unexpected(make_error_code(std::errc::bad_message));
}

stdx::expected<Processor::Result, std::error_code>
ResetConnectionForwarder::ok() {
  auto &src_conn = connection()->server_conn();
  auto &src_protocol = src_conn.protocol();

  auto &dst_conn = connection()->client_conn();
  auto &dst_protocol = dst_conn.protocol();

  auto msg_res =
      ClassicFrame::recv_msg<classic_protocol::borrowed::message::server::Ok>(
          src_conn);
  if (!msg_res) return recv_server_failed(msg_res.error());

  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("reset_connection::ok"));
  }

  auto msg = *msg_res;

  if (!msg.session_changes().empty()) {
    auto track_res = connection()->track_session_changes(
        net::buffer(msg.session_changes()), src_protocol.shared_capabilities());
    if (!track_res) {
      // ignore
    }
  }

  dst_protocol.status_flags(msg.status_flags());

  discard_current_msg(src_conn);

  stage(Stage::SetVars);
  return Result::Again;
}

namespace {
void set_session_var(std::string &q, const std::string &key, const Value &val) {
  if (q.empty()) {
    q = "SET ";
  } else {
    q += ",\n    ";
  }

  q += "@@SESSION." + key + " = " + val.to_string();
}

void set_session_var_if_not_set(
    std::string &q, const ExecutionContext::SystemVariables &sysvars,
    const std::string &key, const Value &value) {
  if (sysvars.get(key) == Value(std::nullopt)) {
    set_session_var(q, key, value);
  }
}

void set_session_var_or_value(std::string &q,
                              const ExecutionContext::SystemVariables &sysvars,
                              const std::string &key,
                              const Value &default_value) {
  auto value = sysvars.get(key);
  if (value == Value(std::nullopt)) {
    set_session_var(q, key, default_value);
  } else {
    set_session_var(q, key, value);
  }
}
}  // namespace

stdx::expected<Processor::Result, std::error_code>
ResetConnectionForwarder::set_vars() {
  auto &sysvars = connection()->execution_context().system_variables();

  std::string stmt;

  const auto need_session_trackers =
      connection()->context().connection_sharing() &&
      connection()->greeting_from_router();

  // must be first, to track all variables that are set.
  if (need_session_trackers) {
    set_session_var_or_value(stmt, sysvars, "session_track_system_variables",
                             Value("*"));
  } else {
    auto var = sysvars.get("session_track_system_variables");
    if (var != Value(std::nullopt)) {
      set_session_var(stmt, "session_track_system_variables", var);
    }
  }

  for (const auto &var : sysvars) {
    // already set earlier.
    if (var.first == "session_track_system_variables") continue;

    // is read-only
    if (var.first == "statement_id") continue;

    set_session_var(stmt, var.first, var.second);
  }

  if (need_session_trackers) {
    set_session_var_if_not_set(stmt, sysvars, "session_track_gtids",
                               Value("OWN_GTID"));
    set_session_var_if_not_set(stmt, sysvars, "session_track_schema",
                               Value("ON"));
    set_session_var_if_not_set(stmt, sysvars, "session_track_state_change",
                               Value("ON"));
    set_session_var_if_not_set(stmt, sysvars, "session_track_transaction_info",
                               Value("CHARACTERISTICS"));
  }

  if (!stmt.empty()) {
    stage(Stage::SetVarsDone);

    if (auto &tr = tracer()) {
      tr.trace(Tracer::Event().stage("reset_connection::set_var"));
    }

    connection()->push_processor(std::make_unique<QuerySender>(
        connection(), stmt, std::make_unique<FailedQueryHandler>(*this, stmt)));
  } else {
    stage(Stage::FetchSysVars);
  }
  return Result::Again;
}

stdx::expected<Processor::Result, std::error_code>
ResetConnectionForwarder::set_vars_done() {
  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("reset_connection::set_var::done"));
  }

  stage(Stage::FetchSysVars);
  return Result::Again;
}

stdx::expected<Processor::Result, std::error_code>
ResetConnectionForwarder::fetch_sys_vars() {
  std::ostringstream oss;

  if (connection()->connection_sharing_possible()) {
    // fetch the sys-vars that aren't known yet.
    for (const auto &expected_var :
         {"collation_connection", "character_set_client", "sql_mode"}) {
      const auto &sys_vars =
          connection()->execution_context().system_variables();
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
    if (auto &tr = tracer()) {
      tr.trace(Tracer::Event().stage("reset_connection::fetch_sys_vars"));
    }

    stage(Stage::FetchSysVarsDone);

    connection()->push_processor(std::make_unique<QuerySender>(
        connection(), oss.str(),
        std::make_unique<SelectSessionVariablesHandler>(connection())));
  } else {
    stage(Stage::SendOk);
  }

  return Result::Again;
}

stdx::expected<Processor::Result, std::error_code>
ResetConnectionForwarder::fetch_sys_vars_done() {
  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("reset_connection::fetch_sys_vars::done"));
  }

  stage(Stage::EndLoop);
  return Result::Again;
}

stdx::expected<Processor::Result, std::error_code>
ResetConnectionForwarder::end_loop() {
  if (round_ == 0) {
    ++round_;

    // reset the "other" server-side connection too.
    if (connection()->expected_server_mode() ==
        mysqlrouter::ServerMode::ReadOnly) {
      if (!connection()->read_write_destination_id().empty()) {
        connection()->stash_server_conn();

        connection()->expected_server_mode(mysqlrouter::ServerMode::ReadWrite);

        stage(Stage::StartLoop);
        return Result::Again;
      }
    } else if (connection()->expected_server_mode() ==
               mysqlrouter::ServerMode::ReadWrite) {
      if (!connection()->read_only_destination_id().empty()) {
        connection()->stash_server_conn();

        connection()->expected_server_mode(mysqlrouter::ServerMode::ReadOnly);

        stage(Stage::StartLoop);
        return Result::Again;
      }
    }
  } else if (round_ == 1) {
    ++round_;

    // ... and switch back to the initial expected-server-mode.
    if (connection()->expected_server_mode() ==
        mysqlrouter::ServerMode::ReadOnly) {
      if (!connection()->read_write_destination_id().empty()) {
        connection()->stash_server_conn();

        connection()->expected_server_mode(mysqlrouter::ServerMode::ReadWrite);
      }
    } else if (connection()->expected_server_mode() ==
               mysqlrouter::ServerMode::ReadWrite) {
      if (!connection()->read_only_destination_id().empty()) {
        connection()->stash_server_conn();

        connection()->expected_server_mode(mysqlrouter::ServerMode::ReadOnly);
      }
    }
  }

  stage(Stage::SendOk);
  return Result::Again;
}

stdx::expected<Processor::Result, std::error_code>
ResetConnectionForwarder::send_ok() {
  auto &dst_conn = connection()->client_conn();

  stage(Stage::Done);

  auto send_res =
      ClassicFrame::send_msg<classic_protocol::borrowed::message::server::Ok>(
          dst_conn, {});
  if (!send_res) return stdx::unexpected(send_res.error());

  return Result::SendToClient;
}
