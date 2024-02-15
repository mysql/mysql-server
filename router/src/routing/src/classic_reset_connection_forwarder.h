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

#ifndef ROUTING_CLASSIC_RESET_CONNECTION_FORWARDER_INCLUDED
#define ROUTING_CLASSIC_RESET_CONNECTION_FORWARDER_INCLUDED

#include "forwarding_processor.h"

class ResetConnectionForwarder : public ForwardingProcessor {
 public:
  using ForwardingProcessor::ForwardingProcessor;

  enum class Stage {
    Command,
    StartLoop,
    Connect,
    Connected,
    Response,
    Ok,
    SetVars,
    SetVarsDone,
    FetchSysVars,
    FetchSysVarsDone,
    EndLoop,
    SendOk,
    Done,
  };

  stdx::expected<Result, std::error_code> process() override;

  void stage(Stage stage) { stage_ = stage; }
  Stage stage() const { return stage_; }

  void failed(
      const std::optional<classic_protocol::message::server::Error> &err) {
    failed_ = err;
  }

  std::optional<classic_protocol::message::server::Error> failed() const {
    return failed_;
  }

 private:
  stdx::expected<Result, std::error_code> command();
  stdx::expected<Result, std::error_code> start_loop();
  stdx::expected<Result, std::error_code> connect();
  stdx::expected<Result, std::error_code> connected();
  stdx::expected<Result, std::error_code> response();
  stdx::expected<Result, std::error_code> ok();
  stdx::expected<Result, std::error_code> set_vars();
  stdx::expected<Result, std::error_code> set_vars_done();
  stdx::expected<Result, std::error_code> fetch_sys_vars();
  stdx::expected<Result, std::error_code> fetch_sys_vars_done();
  stdx::expected<Result, std::error_code> end_loop();
  stdx::expected<Result, std::error_code> send_ok();

  Stage stage_{Stage::Command};

  std::optional<classic_protocol::message::server::Error> failed_;

  int round_{0};
};

#endif
