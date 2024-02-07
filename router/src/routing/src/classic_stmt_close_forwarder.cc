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

#include "classic_stmt_close_forwarder.h"

#include "classic_connection_base.h"
#include "classic_frame.h"
#include "mysql/harness/stdx/expected.h"

stdx::expected<Processor::Result, std::error_code>
StmtCloseForwarder::process() {
  switch (stage()) {
    case Stage::Command:
      return command();
    case Stage::Done:
      return Result::Done;
  }

  harness_assert_this_should_not_execute();
}

stdx::expected<Processor::Result, std::error_code>
StmtCloseForwarder::command() {
  auto &src_conn = connection()->client_conn();
  auto &src_protocol = src_conn.protocol();

  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("stmt_close::command"));
  }

  auto msg_res = ClassicFrame::recv_msg<
      classic_protocol::borrowed::message::client::StmtClose>(src_conn);
  if (!msg_res) return recv_client_failed(msg_res.error());

  // forget everything about a prepared statement.
  src_protocol.prepared_statements().erase(msg_res->statement_id());

  auto &server_conn = connection()->server_conn();
  if (!server_conn.is_open()) {
    auto frame_res = ClassicFrame::ensure_has_full_frame(src_conn);
    if (!frame_res) return recv_client_failed(frame_res.error());

    stage(Stage::Done);
    // discard the recv'ed message as there is ...
    //
    // - no server connection to send it to
    // - and therefore no prepared statement that could be closed on the server.
    //
    // StmtClose also has no way to report errors.
    discard_current_msg(src_conn);

    return Result::Again;
  }

  stage(Stage::Done);

  return forward_client_to_server();
}
