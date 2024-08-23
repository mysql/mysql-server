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

#include "classic_statistics_forwarder.h"

#include "classic_connection_base.h"
#include "classic_frame.h"
#include "classic_lazy_connect.h"
#include "mysql/harness/stdx/expected.h"
#include "mysql/harness/tls_error.h"

stdx::expected<Processor::Result, std::error_code>
StatisticsForwarder::process() {
  switch (stage()) {
    case Stage::Command:
      return command();
    case Stage::Connect:
      return connect();
    case Stage::Connected:
      return connected();
    case Stage::Forward:
      return forward();
    case Stage::ForwardDone:
      return forward_done();
    case Stage::Response:
      return response();
    case Stage::Done:
      return Result::Done;
  }

  harness_assert_this_should_not_execute();
}

stdx::expected<Processor::Result, std::error_code>
StatisticsForwarder::command() {
  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("statistics::command"));
  }

  // reset the warnings from the previous statements.
  connection()->execution_context().diagnostics_area().warnings().clear();

  trace_event_command_ = trace_command(prefix());

  trace_event_connect_and_forward_command_ =
      trace_connect_and_forward_command(trace_event_command_);

  auto &server_conn = connection()->server_conn();
  if (!server_conn.is_open()) {
    stage(Stage::Connect);
  } else {
    trace_event_forward_command_ =
        trace_forward_command(trace_event_connect_and_forward_command_);

    stage(Stage::Forward);
  }
  return Result::Again;
}

stdx::expected<Processor::Result, std::error_code>
StatisticsForwarder::connect() {
  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("statistics::connect"));
  }

  stage(Stage::Connected);
  return mysql_reconnect_start(trace_event_connect_and_forward_command_);
}

stdx::expected<Processor::Result, std::error_code>
StatisticsForwarder::connected() {
  if (reconnect_error().error_code() != 0) {
    auto &src_conn = connection()->client_conn();

    // take the client::command from the connection.
    auto recv_res = ClassicFrame::ensure_has_full_frame(src_conn);
    if (!recv_res) return recv_client_failed(recv_res.error());

    discard_current_msg(src_conn);

    if (auto &tr = tracer()) {
      tr.trace(Tracer::Event().stage("statistics::connect::error"));
    }

    trace_span_end(trace_event_connect_and_forward_command_);
    trace_command_end(trace_event_command_);

    stage(Stage::Done);
    return reconnect_send_error_msg(src_conn);
  }

  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("statistics::connected"));
  }

  trace_event_forward_command_ =
      trace_forward_command(trace_event_connect_and_forward_command_);

  stage(Stage::Forward);
  return Result::Again;
}

stdx::expected<Processor::Result, std::error_code>
StatisticsForwarder::forward() {
  stage(Stage::ForwardDone);
  return forward_client_to_server();
}

stdx::expected<Processor::Result, std::error_code>
StatisticsForwarder::forward_done() {
  stage(Stage::Response);

  trace_span_end(trace_event_forward_command_);
  trace_span_end(trace_event_connect_and_forward_command_);

  return Result::Again;
}

stdx::expected<Processor::Result, std::error_code>
StatisticsForwarder::response() {
  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("statistics::response"));
  }

  trace_command_end(trace_event_command_);

  stage(Stage::Done);

  return forward_server_to_client();
}
