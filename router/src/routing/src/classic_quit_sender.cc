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

#include "classic_quit_sender.h"

#include "classic_connection_base.h"
#include "classic_frame.h"
#include "mysql/harness/logging/logging.h"
#include "mysql/harness/net_ts/impl/socket.h"
#include "mysql/harness/net_ts/socket.h"
#include "mysql/harness/stdx/expected.h"
#include "mysql/harness/tls_error.h"
#include "mysqlrouter/connection_pool.h"
#include "mysqlrouter/connection_pool_component.h"
#include "tracer.h"

IMPORT_LOG_FUNCTIONS()

// sender

stdx::expected<Processor::Result, std::error_code> QuitSender::process() {
  switch (stage()) {
    case Stage::Command:
      return command();
    case Stage::CloseSocket:
      return close_socket();
    case Stage::Done:
      return Result::Done;
  }

  harness_assert_this_should_not_execute();
}

stdx::expected<Processor::Result, std::error_code> QuitSender::command() {
  auto *socket_splicer = connection()->socket_splicer();
  auto *dst_protocol = connection()->server_protocol();
  auto *dst_channel = socket_splicer->server_channel();

  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("quit::command"));
  }

  dst_protocol->seq_id(0xff);

  auto msg_res =
      ClassicFrame::send_msg<classic_protocol::borrowed::message::client::Quit>(
          dst_channel, dst_protocol, {});
  if (!msg_res) return send_server_failed(msg_res.error());

  stage(Stage::CloseSocket);
  return Result::SendToServer;
}

stdx::expected<Processor::Result, std::error_code> QuitSender::close_socket() {
  auto *socket_splicer = connection()->socket_splicer();

  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event()
                 .stage("quit::close")
                 .direction(Tracer::Event::Direction::kServerClose));
  }

  (void)socket_splicer->server_conn().close();

  stage(Stage::Done);
  return Result::Again;
}
