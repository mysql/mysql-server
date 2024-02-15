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

#ifndef ROUTING_CLASSIC_QUERY_FORWARDER_INCLUDED
#define ROUTING_CLASSIC_QUERY_FORWARDER_INCLUDED

#include <system_error>

#include "forwarding_processor.h"
#include "mysqlrouter/classic_protocol_message.h"
#include "sql_parser_state.h"
#include "stmt_classifier.h"

class QueryForwarder : public ForwardingProcessor {
 public:
  using ForwardingProcessor::ForwardingProcessor;

  enum class Stage {
    Command,

    ExplicitCommitConnect,
    ExplicitCommitConnectDone,
    ExplicitCommit,
    ExplicitCommitDone,

    ClassifyQuery,

    SwitchBackend,
    PrepareBackend,

    Connect,
    Connected,

    Forward,
    ForwardDone,

    Response,
    ColumnCount,
    Column,
    ColumnEnd,
    RowOrEnd,
    Row,
    RowEnd,

    LoadData,
    Data,

    Ok,
    Error,

    ResponseDone,
    Done,

    SendQueued,
  };

  static constexpr std::string_view prefix() { return "mysql/query"; }

  stdx::expected<Result, std::error_code> process() override;

  void stage(Stage stage) { stage_ = stage; }
  [[nodiscard]] Stage stage() const { return stage_; }

  void failed(
      const std::optional<classic_protocol::message::server::Error> &err) {
    failed_ = err;
  }

  std::optional<classic_protocol::message::server::Error> failed() const {
    return failed_;
  }

 private:
  stdx::expected<Result, std::error_code> command();
  stdx::expected<Result, std::error_code> explicit_commit_connect();
  stdx::expected<Result, std::error_code> explicit_commit_connect_done();
  stdx::expected<Result, std::error_code> explicit_commit();
  stdx::expected<Result, std::error_code> explicit_commit_done();
  stdx::expected<Result, std::error_code> classify_query();
  stdx::expected<Result, std::error_code> switch_backend();
  stdx::expected<Result, std::error_code> prepare_backend();
  stdx::expected<Result, std::error_code> connect();
  stdx::expected<Result, std::error_code> connected();
  stdx::expected<Result, std::error_code> forward();
  stdx::expected<Result, std::error_code> forward_done();
  stdx::expected<Result, std::error_code> response();
  stdx::expected<Result, std::error_code> load_data();
  stdx::expected<Result, std::error_code> data();

  stdx::expected<Result, std::error_code> column_count();
  stdx::expected<Result, std::error_code> column();
  stdx::expected<Result, std::error_code> column_end();
  stdx::expected<Result, std::error_code> row_or_end();
  stdx::expected<Result, std::error_code> row();
  stdx::expected<Result, std::error_code> row_end();

  stdx::expected<Result, std::error_code> ok();
  stdx::expected<Result, std::error_code> error();
  stdx::expected<Result, std::error_code> response_done();

  stdx::expected<Result, std::error_code> send_queued();

  stdx::expected<void, std::error_code> track_session_changes(
      net::const_buffer session_trackers,
      classic_protocol::capabilities::value_type caps);

  TraceEvent *trace_connect_and_explicit_commit(TraceEvent *parent_span);

  stdx::flags<StmtClassifier> stmt_classified_{};

  Stage stage_{Stage::Command};

  uint64_t columns_left_{0};

  TraceEvent *trace_event_command_{};
  TraceEvent *trace_event_connect_and_explicit_commit_{};
  TraceEvent *trace_event_connect_and_forward_command_{};
  TraceEvent *trace_event_forward_command_{};
  TraceEvent *trace_event_query_result_{};

  std::optional<classic_protocol::message::server::Error> failed_;

  SqlParserState sql_parser_state_;
};

#endif
