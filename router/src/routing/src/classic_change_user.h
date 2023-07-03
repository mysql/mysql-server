/*
  Copyright (c) 2022, Oracle and/or its affiliates.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2.0,
  as published by the Free Software Foundation.

  This program is also distributed with certain software (including
  but not limited to OpenSSL) that is licensed under separate terms,
  as designated in a particular file or component or in included license
  documentation.  The authors of MySQL hereby grant you an additional
  permission to link the program and your derivative works with the
  separately licensed software that they have included with MySQL.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef ROUTING_CLASSIC_CHANGE_USER_INCLUDED
#define ROUTING_CLASSIC_CHANGE_USER_INCLUDED

#include "processor.h"

/**
 * forwards COM_CHANGE_USER from client to the server.
 */
class ChangeUserForwarder : public Processor {
 public:
  using Processor::Processor;

  enum class Stage {
    Command,
    Connect,
    Connected,
    Response,
    Ok,
    Error,
    Done,
  };

  stdx::expected<Result, std::error_code> process() override;

  void stage(Stage stage) { stage_ = stage; }
  [[nodiscard]] Stage stage() const { return stage_; }

 private:
  stdx::expected<Result, std::error_code> command();
  stdx::expected<Result, std::error_code> connect();
  stdx::expected<Result, std::error_code> connected();
  stdx::expected<Result, std::error_code> response();
  stdx::expected<Result, std::error_code> ok();
  stdx::expected<Result, std::error_code> error();

  Stage stage_{Stage::Command};
};

/**
 * sends COM_CHANGE_USER from router to the server.
 */
class ChangeUserSender : public Processor {
 public:
  using Processor::Processor;

  ChangeUserSender(MysqlRoutingClassicConnection *conn, bool in_handshake)
      : Processor(conn), in_handshake_{in_handshake} {}

  enum class Stage {
    Command,
    InitialResponse,
    FinalResponse,
    Ok,
    Error,
    Done,
  };

  stdx::expected<Result, std::error_code> process() override;

  void stage(Stage stage) { stage_ = stage; }
  [[nodiscard]] Stage stage() const { return stage_; }

 private:
  stdx::expected<Result, std::error_code> command();
  stdx::expected<Result, std::error_code> initial_response();
  stdx::expected<Result, std::error_code> final_response();
  stdx::expected<Result, std::error_code> ok();
  stdx::expected<Result, std::error_code> error();

  Stage stage_{Stage::Command};

  bool in_handshake_{false};

  std::optional<classic_protocol::message::client::ChangeUser> change_user_msg_;
};

#endif
