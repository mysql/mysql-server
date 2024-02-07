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

#ifndef ROUTING_CLASSIC_AWAIT_CLIENT_OR_SERVER_PROCESSOR_INCLUDED
#define ROUTING_CLASSIC_AWAIT_CLIENT_OR_SERVER_PROCESSOR_INCLUDED

#include "processor.h"

class AwaitClientOrServerProcessor : public BasicProcessor {
 public:
  enum class AwaitResult {
    ClientReadable,
    ServerReadable,
  };

  AwaitClientOrServerProcessor(
      MysqlRoutingClassicConnectionBase *conn,
      std::function<void(stdx::expected<AwaitResult, std::error_code>)> on_done)
      : BasicProcessor(conn), on_done_(std::move(on_done)) {}

  stdx::expected<Result, std::error_code> process() override;

 private:
  enum class Stage {
    Init,
    WaitBoth,
    WaitClientCancelled,
    WaitServerCancelled,
    Done,
  };

  void stage(Stage stage) { stage_ = stage; }
  Stage stage() const { return stage_; }

  stdx::expected<Result, std::error_code> init();
  stdx::expected<Result, std::error_code> wait_both();
  stdx::expected<Result, std::error_code> wait_client_cancelled();
  stdx::expected<Result, std::error_code> wait_server_cancelled();

  Stage stage_{Stage::Init};

  std::function<void(stdx::expected<AwaitResult, std::error_code>)> on_done_;
};

#endif
