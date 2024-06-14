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

#ifndef ROUTING_CLASSIC_STMT_PREPARE_FORWARDER_INCLUDED
#define ROUTING_CLASSIC_STMT_PREPARE_FORWARDER_INCLUDED

#include "forwarding_processor.h"

#include "mysqlrouter/classic_prepared_statement.h"

class StmtPrepareForwarder : public ForwardingProcessor {
 public:
  using ForwardingProcessor::ForwardingProcessor;

  enum class Stage {
    Command,

    ForbidCommand,

    PrepareBackend,

    Connect,
    Connected,
    Forward,
    ForwardDone,
    Response,
    Column,
    EndOfColumns,
    Param,
    EndOfParams,
    Ok,
    OkDone,
    Error,
    Done,
  };

  static std::string_view prefix() { return "mysql/stmt_prepare"; }

  stdx::expected<Result, std::error_code> process() override;

  void stage(Stage stage) { stage_ = stage; }
  Stage stage() const { return stage_; }

 private:
  stdx::expected<Result, std::error_code> command();
  stdx::expected<Result, std::error_code> forbid_command();
  stdx::expected<Result, std::error_code> prepare_backend();
  stdx::expected<Result, std::error_code> connect();
  stdx::expected<Result, std::error_code> connected();
  stdx::expected<Result, std::error_code> forward();
  stdx::expected<Result, std::error_code> forward_done();
  stdx::expected<Result, std::error_code> response();
  stdx::expected<Result, std::error_code> ok();
  stdx::expected<Result, std::error_code> column();
  stdx::expected<Result, std::error_code> end_of_columns();
  stdx::expected<Result, std::error_code> param();
  stdx::expected<Result, std::error_code> end_of_params();
  stdx::expected<Result, std::error_code> ok_done();
  stdx::expected<Result, std::error_code> error();

  bool has_more_messages() const;

  Stage stage_{Stage::Command};

  size_t columns_left_{0};
  size_t params_left_{0};

  uint16_t stmt_id_{};
  PreparedStatement prep_stmt_{};

  TraceEvent *trace_event_command_{nullptr};
  TraceEvent *trace_event_connect_and_forward_command_{nullptr};
  TraceEvent *trace_event_forward_command_{nullptr};
};

#endif
