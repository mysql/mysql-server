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

#ifndef ROUTING_CLASSIC_QUERY_SENDER_INCLUDED
#define ROUTING_CLASSIC_QUERY_SENDER_INCLUDED

#include <system_error>

#include "processor.h"
#include "stmt_classifier.h"

class QuerySender : public Processor {
 public:
  using Processor::Processor;

  class Handler {
   public:
    virtual ~Handler() = default;

    virtual void on_column_count(uint64_t count) { (void)count; }

    virtual void on_column(
        const classic_protocol::message::server::ColumnMeta &column) {
      (void)column;
    }
    virtual void on_row(const classic_protocol::message::server::Row &row) {
      (void)row;
    }
    virtual void on_row_end(const classic_protocol::message::server::Eof &eof) {
      (void)eof;
    }
    virtual void on_ok(const classic_protocol::message::server::Ok &ok) {
      (void)ok;
    }
    virtual void on_error(const classic_protocol::message::server::Error &err) {
      (void)err;
    }
  };

  QuerySender(MysqlRoutingClassicConnectionBase *conn, std::string stmt)
      : Processor(conn), stmt_{std::move(stmt)} {}

  QuerySender(MysqlRoutingClassicConnectionBase *conn, std::string stmt,
              std::unique_ptr<Handler> handler)
      : Processor(conn), stmt_{std::move(stmt)}, handler_(std::move(handler)) {}

  enum class Stage {
    Command,

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

    Done,
  };

  stdx::expected<Result, std::error_code> process() override;

  void stage(Stage stage) { stage_ = stage; }
  Stage stage() const { return stage_; }

 private:
  stdx::expected<Result, std::error_code> command();
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

  stdx::expected<void, std::error_code> track_session_changes(
      net::const_buffer session_trackers,
      classic_protocol::capabilities::value_type caps);

  Stage stage_{Stage::Command};

  std::string stmt_;
  uint64_t columns_left_{0};

  std::unique_ptr<Handler> handler_;
};

#endif
