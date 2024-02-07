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

#ifndef ROUTING_CLASSIC_CHANGE_USER_SENDER_INCLUDED
#define ROUTING_CLASSIC_CHANGE_USER_SENDER_INCLUDED

#include "forwarding_processor.h"

/**
 * sends COM_CHANGE_USER from router to the server.
 */
class ChangeUserSender : public ForwardingProcessor {
 public:
  using ForwardingProcessor::ForwardingProcessor;

  ChangeUserSender(
      MysqlRoutingClassicConnectionBase *conn, bool in_handshake,
      std::function<void(const classic_protocol::message::server::Error &)>
          on_error,
      TraceEvent *parent_event)
      : ForwardingProcessor(conn),
        in_handshake_{in_handshake},
        on_error_(std::move(on_error)),
        parent_event_(parent_event) {}

  enum class Stage {
    Command,
    InitialResponse,
    FinalResponse,
    Ok,
    Error,
    Done,
  };

  static constexpr std::string_view prefix() { return "change_user"; }

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

  std::function<void(const classic_protocol::message::server::Error &err)>
      on_error_;

  TraceEvent *parent_event_{};
  TraceEvent *trace_event_command_{};
};

#endif
