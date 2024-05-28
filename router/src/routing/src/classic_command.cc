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

#include "classic_command.h"

#include <charconv>
#include <memory>  // make_unique
#include <string>

#include "await_client_or_server.h"
#include "basic_protocol_splicer.h"
#include "classic_binlog_dump_forwarder.h"
#include "classic_change_user_forwarder.h"
#include "classic_clone_forwarder.h"
#include "classic_connection_base.h"
#include "classic_debug_forwarder.h"
#include "classic_frame.h"
#include "classic_init_schema_forwarder.h"
#include "classic_kill_forwarder.h"
#include "classic_list_fields_forwarder.h"
#include "classic_ping_forwarder.h"
#include "classic_query_forwarder.h"
#include "classic_query_sender.h"
#include "classic_quit_forwarder.h"
#include "classic_register_replica_forwarder.h"
#include "classic_reload_forwarder.h"
#include "classic_reset_connection_forwarder.h"
#include "classic_set_option_forwarder.h"
#include "classic_statistics_forwarder.h"
#include "classic_stmt_close_forwarder.h"
#include "classic_stmt_execute_forwarder.h"
#include "classic_stmt_fetch_forwarder.h"
#include "classic_stmt_param_append_data_forwarder.h"
#include "classic_stmt_prepare_forwarder.h"
#include "classic_stmt_reset_forwarder.h"
#include "harness_assert.h"
#include "hexify.h"
#include "mysql/harness/logging/logging.h"
#include "mysql/harness/stdx/expected.h"
#include "mysql/harness/tls_error.h"
#include "mysqld_error.h"
#include "mysqlrouter/connection_pool.h"
#include "mysqlrouter/connection_pool_component.h"
#include "mysqlrouter/utils.h"  // to_string
#include "processor.h"
#include "tracer.h"

IMPORT_LOG_FUNCTIONS()

using mysql_harness::hexify;

namespace {
template <class P>
stdx::expected<Processor::Result, std::error_code> push_processor(
    MysqlRoutingClassicConnectionBase *conn) {
  conn->push_processor(std::make_unique<P>(conn));

  return Processor::Result::Again;
}
}  // namespace

stdx::expected<Processor::Result, std::error_code> CommandProcessor::process() {
  switch (stage()) {
    case Stage::IsAuthed:
      return is_authed();
    case Stage::WaitBoth:
      return wait_both();
    case Stage::Command:
      return command();
    case Stage::FetchDiagnosticArea:
      return fetch_diagnostic_area();
    case Stage::Done:
      return Result::Done;
  }

  harness_assert_this_should_not_execute();
}

stdx::expected<Processor::Result, std::error_code>
CommandProcessor::is_authed() {
  // if authentication is lost, close the connection.
  stage(connection()->authenticated() ? Stage::FetchDiagnosticArea
                                      : Stage::Done);

  return Result::Again;
}

void CommandProcessor::client_idle_timeout() {
  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("client::idle::timeout"));
  }

  auto pool_res = pool_server_connection();
  if (!pool_res) return;

  if (auto &tr = tracer()) {
    bool connection_was_pooled = *pool_res;

    tr.trace(Tracer::Event().stage(connection_was_pooled
                                       ? "client::idle::pooled"
                                       : "client::idle::pool_full"));
  }
}

class ShowWarningsHandler : public QuerySender::Handler {
 public:
  ShowWarningsHandler(MysqlRoutingClassicConnectionBase *connection)
      : connection_(connection) {}

  void on_column_count(uint64_t count) override {
    col_count_ = count;

    if (col_count_ != 3) {
      connection_->some_state_changed(true);
    } else {
      connection_->execution_context().diagnostics_area().warnings().clear();
    }
  }

  void on_column(
      const classic_protocol::message::server::ColumnMeta &col) override {
    switch (col_count_) {
      case 0:
        if (col.name() != "Level") {
          something_failed_ = true;
        }
        break;
      case 1:
        if (col.name() != "Code") {
          something_failed_ = true;
        }
        break;
      case 2:
        if (col.name() != "Message") {
          something_failed_ = true;
        }
        break;
      default:
        // more columns is ok.
        break;
    }

    ++col_count_;
  }

  void on_row(const classic_protocol::message::server::Row &row) override {
    if (something_failed_) return;

    auto it = row.begin();  // row[0]

    if (!(*it).has_value()) {
      something_failed_ = true;
      return;
    }

    std::string level = (*it).value();

    ++it;  // row[1]

    uint64_t code;
    {
      const auto &fld = *it;
      if (!fld) {
        something_failed_ = true;
        return;
      }

      auto conv_res =
          std::from_chars(fld->data(), fld->data() + fld->size(), code);

      if (conv_res.ec != std::errc{}) {
        something_failed_ = true;
        return;
      }
    }

    ++it;  // row[2]

    if (!(*it).has_value()) {
      something_failed_ = true;
      return;
    }

    std::string msg = (*it).value();

    connection_->execution_context().diagnostics_area().warnings().emplace_back(
        level, code, msg);
  }

  void on_row_end(
      const classic_protocol::message::server::Eof & /* eof */) override {
    if (something_failed_) {
      // something failed when parsing the resultset. Disable sharing for now.
      connection_->some_state_changed(true);
    } else {
      // all rows received, diagnostics_area fully synced.
      connection_->diagnostic_area_changed(false);
    }
  }

  void on_ok(const classic_protocol::message::server::Ok & /* ok */) override {
    // ok, shouldn't happen. Disable sharing for now.
    connection_->some_state_changed(true);
  }

  void on_error(
      const classic_protocol::message::server::Error & /* err */) override {
    // error, shouldn't happen. Disable sharing for now.
    connection_->some_state_changed(true);
  }

 private:
  uint64_t col_count_{};
  uint64_t col_cur_{};
  MysqlRoutingClassicConnectionBase *connection_;

  bool something_failed_{false};
};

class SelectSessionCollationConnectionHandler : public QuerySender::Handler {
 public:
  SelectSessionCollationConnectionHandler(
      MysqlRoutingClassicConnectionBase *connection)
      : connection_(connection) {}

  void on_column_count(uint64_t count) override {
    col_count_ = count;

    if (col_count_ != 1) {
      something_failed_ = true;
    }
  }

  void on_column(
      const classic_protocol::message::server::ColumnMeta &col) override {
    if (something_failed_) return;

    if (col.name() != "@@SESSION.collation_connection") {
      something_failed_ = true;
    }
  }

  void on_row(const classic_protocol::message::server::Row &row) override {
    if (something_failed_) return;

    auto it = row.begin();  // row[0]

    if (!it->has_value()) {
      something_failed_ = true;
      return;
    }

    collation_connection_ = *it;
  }

  void on_row_end(
      const classic_protocol::message::server::Eof & /* eof */) override {
    if (something_failed_) {
      // something failed when parsing the resultset. Disable sharing for now.
      connection_->some_state_changed(true);
    } else {
      // all rows received,
      connection_->client_protocol().system_variables().set(
          "collation_connection", collation_connection_);
      connection_->server_protocol().system_variables().set(
          "collation_connection", collation_connection_);

      connection_->collation_connection_maybe_dirty(false);
    }
  }

  void on_ok(const classic_protocol::message::server::Ok & /* ok */) override {
    // ok, shouldn't happen. Disable sharing for now.
    connection_->some_state_changed(true);
  }

  void on_error(
      const classic_protocol::message::server::Error & /* err */) override {
    // error, shouldn't happen. Disable sharing for now.
    connection_->some_state_changed(true);
  }

 private:
  uint64_t col_count_{};
  uint64_t col_cur_{};
  MysqlRoutingClassicConnectionBase *connection_;

  bool something_failed_{false};

  std::optional<std::string> collation_connection_{std::nullopt};
};

stdx::expected<Processor::Result, std::error_code>
CommandProcessor::fetch_diagnostic_area() {
  auto &server_conn = connection()->server_conn();

  if (connection()->disconnect_requested()) {
    stage(Stage::Done);
    return Result::Again;
  }

  if (server_conn.is_open() && connection()->connection_sharing_allowed()) {
    if (connection()->diagnostic_area_changed()) {
      // inject a SHOW WARNINGS.
      connection()->push_processor(std::make_unique<QuerySender>(
          connection(), "SHOW WARNINGS",
          std::make_unique<ShowWarningsHandler>(connection())));

      return Result::Again;
    }

    if (connection()->collation_connection_maybe_dirty()) {
      connection()->push_processor(std::make_unique<QuerySender>(
          connection(), "SELECT @@SESSION.collation_connection",
          std::make_unique<SelectSessionCollationConnectionHandler>(
              connection())));

      return Result::Again;
    }

    // make the connection available to others.
    connection()->stash_server_conn();
  }

  stage(Stage::Command);
  return Result::Again;
}

/**
 * wait for an read-event from client and server at the same time.
 *
 * two async-reads have been started, which both will call wait_both(). Only one
 * of the two should continue.
 *
 * To ensure that event handlers are properly synchronized:
 *
 * - the first returning event, cancels the other waiter and leaves without
 *   "returning" (::Void)
 * - the cancelled side, continues with executing.
 */
stdx::expected<Processor::Result, std::error_code>
CommandProcessor::wait_both() {
  if (wait_both_result_) {
    switch (*wait_both_result_) {
      case AwaitClientOrServerProcessor::AwaitResult::ClientReadable:
        stage(Stage::Command);

        return Result::Again;
      case AwaitClientOrServerProcessor::AwaitResult::ServerReadable: {
        auto &src_conn = connection()->server_conn();

        auto read_res = ClassicFrame::ensure_has_msg_prefix(src_conn);
        if (!read_res) return recv_server_failed(read_res.error());

        stage(Stage::Done);

        if (auto &tr = tracer()) {
          tr.trace(Tracer::Event().stage("server::error"));
        }

        // should be a Error packet.
        return forward_server_to_client();
      }
    }

    harness_assert_this_should_not_execute();
  } else {
    return stdx::unexpected(wait_both_result_.error());
  }
}

stdx::expected<Processor::Result, std::error_code> CommandProcessor::command() {
  auto &src_conn = connection()->client_conn();
  auto &src_channel = src_conn.channel();
  auto &src_protocol = src_conn.protocol();

  auto &server_conn = connection()->server_conn();

  if (connection()->disconnect_requested()) {
    stage(Stage::Done);
    return Result::Again;
  }

  auto read_res = ClassicFrame::ensure_has_msg_prefix(src_conn);
  if (!read_res) {
    // nothing to read. Wait for
    //
    // 1. data
    // 2. wait_timeout to drop the connection
    // 3. multiplex-timeout to move the server side connection to the pool
    auto ec = read_res.error();

    if (ec == std::errc::operation_would_block || ec == TlsErrc::kWantRead) {
      if (server_conn.is_open()) {
        // client and server connection open.
        //
        // watch server-side for connection-close

        connection()->push_processor(
            std::make_unique<AwaitClientOrServerProcessor>(
                connection(),
                [this](auto result) { wait_both_result_ = result; }));

        stage(Stage::WaitBoth);

        return Result::Again;
      }

      return Result::RecvFromClient;
    }

    if (ec == TlsErrc::kZeroReturn) {
      // close the connection without a quit.
      stage(Stage::Done);
      return Result::Again;
    }

    return recv_client_failed(ec);
  }

  const uint8_t msg_type = src_protocol.current_msg_type().value();

  connection()->client_protocol().seq_id(src_protocol.current_frame()->seq_id_);

  namespace client = classic_protocol::message::client;

  enum class Msg {
    Quit = ClassicFrame::cmd_byte<client::Quit>(),
    InitSchema = ClassicFrame::cmd_byte<client::InitSchema>(),
    Query = ClassicFrame::cmd_byte<client::Query>(),
    ListFields = ClassicFrame::cmd_byte<client::ListFields>(),
    Reload = ClassicFrame::cmd_byte<client::Reload>(),
    Statistics = ClassicFrame::cmd_byte<client::Statistics>(),
    // ProcessInfo =
    // ClassicFrame::cmd_byte<classic_protocol::message::client::ProcessInfo>(),
    Kill = ClassicFrame::cmd_byte<client::Kill>(),
    Debug = ClassicFrame::cmd_byte<client::Debug>(),
    Ping = ClassicFrame::cmd_byte<client::Ping>(),
    ChangeUser = ClassicFrame::cmd_byte<client::ChangeUser>(),
    BinlogDump = ClassicFrame::cmd_byte<client::BinlogDump>(),
    RegisterReplica = ClassicFrame::cmd_byte<client::RegisterReplica>(),
    StmtPrepare = ClassicFrame::cmd_byte<client::StmtPrepare>(),
    StmtExecute = ClassicFrame::cmd_byte<client::StmtExecute>(),
    StmtParamAppendData = ClassicFrame::cmd_byte<client::StmtParamAppendData>(),
    StmtClose = ClassicFrame::cmd_byte<client::StmtClose>(),
    StmtReset = ClassicFrame::cmd_byte<client::StmtReset>(),
    SetOption = ClassicFrame::cmd_byte<client::SetOption>(),
    StmtFetch = ClassicFrame::cmd_byte<client::StmtFetch>(),
    BinlogDumpGtid = ClassicFrame::cmd_byte<client::BinlogDumpGtid>(),
    ResetConnection = ClassicFrame::cmd_byte<client::ResetConnection>(),
    Clone = ClassicFrame::cmd_byte<client::Clone>(),
    // SubscribeGroupReplicationStream = ClassicFrame::cmd_byte<
    //     classic_protocol::message::client::SubscribeGroupReplicationStream>(),
  };

  // after the command is processed, check if the connection is still
  // authenticated.
  //
  // - change-user may have failed.
  // - a reconnect may have failed.

  stage(Stage::IsAuthed);

  // init the command tracer.
  connection()->events().active(
      connection()->client_protocol().trace_commands());

  // The query processor handles SHOW WARNINGS which fetches the events.
  if (Msg{msg_type} != Msg::Query) connection()->events().clear();

  // reset the seq-id of the server side as this is a new command.
  connection()->server_protocol().seq_id(0xff);

  switch (Msg{msg_type}) {
    case Msg::Quit:
      stage(Stage::Done);  // after Quit is done, leave.
      return push_processor<QuitForwarder>(connection());
    case Msg::InitSchema:
      return push_processor<InitSchemaForwarder>(connection());
    case Msg::Query:
      return push_processor<QueryForwarder>(connection());
    case Msg::ListFields:
      return push_processor<ListFieldsForwarder>(connection());
    case Msg::ChangeUser:
      return push_processor<ChangeUserForwarder>(connection());
    case Msg::Ping:
      return push_processor<PingForwarder>(connection());
    case Msg::ResetConnection:
      return push_processor<ResetConnectionForwarder>(connection());
    case Msg::Kill:
      return push_processor<KillForwarder>(connection());
    case Msg::Debug:
      return push_processor<DebugForwarder>(connection());
    case Msg::Reload:
      return push_processor<ReloadForwarder>(connection());
    case Msg::Statistics:
      return push_processor<StatisticsForwarder>(connection());
    case Msg::StmtPrepare:
      return push_processor<StmtPrepareForwarder>(connection());
    case Msg::StmtExecute:
      return push_processor<StmtExecuteForwarder>(connection());
    case Msg::StmtClose:
      return push_processor<StmtCloseForwarder>(connection());
    case Msg::StmtFetch:
      return push_processor<StmtFetchForwarder>(connection());
    case Msg::SetOption:
      return push_processor<SetOptionForwarder>(connection());
    case Msg::StmtReset:
      return push_processor<StmtResetForwarder>(connection());
    case Msg::StmtParamAppendData:
      return push_processor<StmtParamAppendDataForwarder>(connection());
    case Msg::Clone:
      return push_processor<CloneForwarder>(connection());
    case Msg::BinlogDump:
    case Msg::BinlogDumpGtid:
      return push_processor<BinlogDumpForwarder>(connection());
    case Msg::RegisterReplica:
      return push_processor<RegisterReplicaForwarder>(connection());
  }

  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("cmd::command"));
  }

  // unknown command
  // drain the current command from the recv-buffers.
  (void)ClassicFrame::ensure_has_full_frame(src_conn);

  log_debug("client sent unknown command: %s",
            hexify(src_channel.recv_plain_view()).c_str());

  // try to discard the current message.
  //
  // if the current message isn't received completely yet, drop the connection
  // after sending the error-message.
  const auto discard_res = discard_current_msg(src_conn);

  const auto send_res = ClassicFrame::send_msg<
      classic_protocol::borrowed::message::server::Error>(
      src_conn, {ER_UNKNOWN_COM_ERROR,
                 "Unknown command " + std::to_string(msg_type), "HY000"});
  if (!discard_res || !send_res) {
    stage(Stage::Done);  // closes the connection after the error-msg was sent.

    return Result::SendToClient;
  }

  return Result::SendToClient;
}
