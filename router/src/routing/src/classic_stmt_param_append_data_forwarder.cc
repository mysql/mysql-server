/*
  Copyright (c) 2023, Oracle and/or its affiliates.

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
  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("stmt_param_append_data::command"));
  }

  auto &server_conn = connection()->socket_splicer()->server_conn();
  if (!server_conn.is_open()) {
    auto *src_channel = connection()->socket_splicer()->client_channel();
    auto *src_protocol = connection()->client_protocol();

    auto frame_res =
        ClassicFrame::ensure_has_full_frame(src_channel, src_protocol);
    if (!frame_res) return recv_client_failed(frame_res.error());

    stage(Stage::Done);

    // discard the recv'ed message as there is ...
    //
    // - no server connection to send it to
    // - and therefore no prepared statement that could be closed on the server.
    //
    // StmtParamAppendData also has no way to report errors.
    discard_current_msg(src_channel, src_protocol);

    return Result::Again;
  } else {
    stage(Stage::Done);

    return forward_client_to_server();
  }
}
