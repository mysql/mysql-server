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

#include "classic_stmt_param_append_data_forwarder.h"

#include "classic_connection_base.h"
#include "classic_frame.h"
#include "mysql/harness/stdx/expected.h"
#include "mysql/harness/tls_error.h"

stdx::expected<Processor::Result, std::error_code>
StmtParamAppendDataForwarder::process() {
  switch (stage()) {
    case Stage::Command:
      return command();
    case Stage::Done:
      return Result::Done;
  }

  harness_assert_this_should_not_execute();
}

stdx::expected<Processor::Result, std::error_code>
StmtParamAppendDataForwarder::command() {
  auto &src_conn = connection()->client_conn();
  auto &src_protocol = src_conn.protocol();

  if (auto &tr = tracer()) {
    // NOTE: as the recv_msg<> of StmtExecute is only called when the tracer is
    // enabled, handle it the same way with StmtParamAppendData.
    //
    auto msg_res = ClassicFrame::recv_msg<
        classic_protocol::borrowed::message::client::StmtParamAppendData>(
        src_conn);
    if (!msg_res) {
      // discard the recv'ed message as there is ...
      //
      // - no server connection to send it to
      // - and therefore no prepared statement that could be closed on the
      // server.
      //
      // StmtParamAppendData also has no way to report errors.
      stage(Stage::Done);

      discard_current_msg(src_conn);

      return Result::Again;
    }

    auto msg = *msg_res;

    tr.trace(
        Tracer::Event().stage("stmt_param_append_data::command: stmt-id: " +
                              std::to_string(msg.statement_id()) +
                              ", param-id: " + std::to_string(msg.param_id())));

    // track that this parameter was already sent.
    auto it = src_protocol.prepared_statements().find(msg.statement_id());
    if (it != src_protocol.prepared_statements().end()) {
      // found
      if (msg.param_id() < it->second.parameters.size()) {
        it->second.parameters[msg.param_id()].param_already_sent = true;
      }
    }
  }

  auto &server_conn = connection()->server_conn();
  if (!server_conn.is_open()) {
    auto frame_res = ClassicFrame::ensure_has_full_frame(src_conn);
    if (!frame_res) return recv_client_failed(frame_res.error());

    stage(Stage::Done);

    discard_current_msg(src_conn);

    return Result::Again;
  }

  stage(Stage::Done);

  return forward_client_to_server();
}
