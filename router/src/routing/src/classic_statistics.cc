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

#include "classic_statistics.h"

#include "classic_connection.h"
#include "classic_forwarder.h"
#include "classic_frame.h"
#include "classic_lazy_connect.h"
#include "mysql/harness/stdx/expected.h"
#include "mysql/harness/tls_error.h"
#include "tracer.h"

stdx::expected<Processor::Result, std::error_code>
StatisticsForwarder::process() {
  switch (stage()) {
    case Stage::Command:
      return command();
    case Stage::Connect:
      return connect();
    case Stage::Connected:
      return connected();
    case Stage::Response:
      return response();
    case Stage::Done:
      return Result::Done;
  }

  harness_assert_this_should_not_execute();
}

stdx::expected<Processor::Result, std::error_code>
StatisticsForwarder::command() {
  trace(Tracer::Event().stage("statistics::command"));

  auto &server_conn = connection()->socket_splicer()->server_conn();
  if (!server_conn.is_open()) {
    stage(Stage::Connect);
    return Result::Again;
  } else {
    stage(Stage::Response);
    return forward_client_to_server();
  }
}

stdx::expected<Processor::Result, std::error_code>
StatisticsForwarder::connect() {
  trace(Tracer::Event().stage("statistics::connect"));

  stage(Stage::Connected);

  connection()->push_processor(std::make_unique<LazyConnector>(
      connection(), false /* not in handshake */));

  return Result::Again;
}

stdx::expected<Processor::Result, std::error_code>
StatisticsForwarder::connected() {
  auto &server_conn = connection()->socket_splicer()->server_conn();
  if (!server_conn.is_open()) {
    // Connector sent an server::Error already.
    auto *socket_splicer = connection()->socket_splicer();
    auto src_channel = socket_splicer->client_channel();
    auto src_protocol = connection()->client_protocol();

    // take the client::command from the connection.
    auto recv_res =
        ClassicFrame::ensure_has_full_frame(src_channel, src_protocol);
    if (!recv_res) return recv_client_failed(recv_res.error());

    discard_current_msg(src_channel, src_protocol);

    trace(Tracer::Event().stage("statistics::error"));

    stage(Stage::Done);
    return Result::Again;
  }

  trace(Tracer::Event().stage("statistics::connected"));

  stage(Stage::Response);
  return forward_client_to_server();
}

stdx::expected<Processor::Result, std::error_code>
StatisticsForwarder::response() {
  trace(Tracer::Event().stage("statistics::response"));

  stage(Stage::Done);

  return forward_server_to_client();
}
