/*
  Copyright (c) 2022, 2024, Oracle and/or its affiliates.

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

#include "processor.h"

#include "classic_connection_base.h"
#include "classic_frame.h"
#include "mysql/harness/logging/logging.h"
#include "mysql/harness/stdx/expected.h"
#include "mysql/harness/tls_error.h"
#include "mysqlrouter/utils.h"  // to_string

IMPORT_LOG_FUNCTIONS()

stdx::expected<Processor::Result, std::error_code>
Processor::send_server_failed(std::error_code ec) {
  // don't call finish() as the calling loop() will call it for us when the
  // error is returned.
  connection()->send_server_failed(ec, false);

  return stdx::unexpected(ec);
}

stdx::expected<Processor::Result, std::error_code>
Processor::recv_server_failed(std::error_code ec) {
  if (ec == TlsErrc::kWantRead) return Result::RecvFromServer;

  // don't call finish() as the calling loop() will call it for us when the
  // error is returned.
  connection()->recv_server_failed(ec, false);

  return stdx::unexpected(ec);
}

stdx::expected<Processor::Result, std::error_code>
Processor::send_client_failed(std::error_code ec) {
  // don't call finish() as the calling loop() will call it for us when the
  // error is returned.
  connection()->send_client_failed(ec, false);

  return stdx::unexpected(ec);
}

stdx::expected<Processor::Result, std::error_code>
Processor::recv_client_failed(std::error_code ec) {
  if (ec == TlsErrc::kWantRead) return Result::RecvFromClient;

  // don't call finish() as the calling loop() will call it for us when the
  // error is returned.
  connection()->recv_client_failed(ec, false);

  return stdx::unexpected(ec);
}

stdx::expected<Processor::Result, std::error_code>
Processor::server_socket_failed(std::error_code ec) {
  // don't call finish() as the calling loop() will call it for us when the
  // error is returned.
  connection()->server_socket_failed(ec, false);

  return stdx::unexpected(ec);
}

stdx::expected<Processor::Result, std::error_code>
Processor::client_socket_failed(std::error_code ec) {
  // don't call finish() as the calling loop() will call it for us when the
  // error is returned.
  connection()->client_socket_failed(ec, false);

  return stdx::unexpected(ec);
}

stdx::expected<void, std::error_code> Processor::discard_current_msg(
    Channel &src_channel, ClassicProtocolState &src_protocol) {
  auto &recv_buf = src_channel.recv_plain_view();

  do {
    auto &opt_current_frame = src_protocol.current_frame();
    if (!opt_current_frame) return {};

    auto current_frame = *opt_current_frame;

    if (recv_buf.size() < current_frame.frame_size_) {
      // received message is incomplete.
      return stdx::unexpected(make_error_code(std::errc::bad_message));
    }
    if (current_frame.forwarded_frame_size_ != 0) {
      // partially forwarded already.
      return stdx::unexpected(make_error_code(std::errc::invalid_argument));
    }

    src_channel.consume_plain(current_frame.frame_size_);

    auto msg_has_more_frames = current_frame.frame_size_ == (0xffffff + 4);

    // unset current frame and also current-msg
    src_protocol.current_frame().reset();

    if (!msg_has_more_frames) break;

    auto hdr_res = ClassicFrame::ensure_frame_header(src_channel, src_protocol);
    if (!hdr_res) {
      return stdx::unexpected(hdr_res.error());
    }
  } while (true);

  src_protocol.current_msg_type().reset();

  return {};
}

void Processor::log_fatal_error_code(const char *msg, std::error_code ec) {
  log_error("%s: %s (%s:%d)", msg, ec.message().c_str(), ec.category().name(),
            ec.value());
}

void Processor::trace(Tracer::Event event) {
  return connection()->trace(std::move(event));
}

Tracer &Processor::tracer() { return connection()->tracer(); }

TraceEvent *Processor::trace_span(TraceEvent *parent_span,
                                  const std::string_view &prefix) {
  if (parent_span == nullptr) return nullptr;

  return std::addressof(parent_span->events.emplace_back(std::string(prefix)));
}

void Processor::trace_span_end(TraceEvent *event,
                               TraceEvent::StatusCode status_code) {
  if (event == nullptr) return;

  event->status_code = status_code;
  event->end_time = std::chrono::steady_clock::now();
}

TraceEvent *Processor::trace_command(const std::string_view &prefix) {
  if (!connection()->events().active()) return nullptr;

  auto *parent_span = std::addressof(connection()->events());

  if (parent_span == nullptr) return nullptr;

  return std::addressof(
      parent_span->events().emplace_back(std::string(prefix)));
}

TraceEvent *Processor::trace_connect_and_forward_command(
    TraceEvent *parent_span) {
  auto *ev = trace_span(parent_span, "mysql/connect_and_forward");
  if (ev == nullptr) return nullptr;

  trace_set_connection_attributes(ev);

  return ev;
}

TraceEvent *Processor::trace_connect(TraceEvent *parent_span) {
  return trace_span(parent_span, "mysql/connect");
}

void Processor::trace_set_connection_attributes(TraceEvent *ev) {
  auto &server_conn = connection()->server_conn();
  ev->attrs.emplace_back("mysql.remote.is_connected", server_conn.is_open());

  if (server_conn.is_open()) {
    if (auto ep = connection()->destination_endpoint()) {
      ev->attrs.emplace_back("mysql.remote.endpoint",
                             mysqlrouter::to_string(*ep));
    }
    ev->attrs.emplace_back(
        "mysql.remote.connection_id",
        static_cast<int64_t>(
            server_conn.protocol().server_greeting()->connection_id()));
    ev->attrs.emplace_back("db.name", server_conn.protocol().schema());
  }
}

TraceEvent *Processor::trace_forward_command(TraceEvent *parent_span) {
  return trace_span(parent_span, "mysql/forward");
}

void Processor::trace_command_end(TraceEvent *event,
                                  TraceEvent::StatusCode status_code) {
  if (event == nullptr) return;

  const auto allowed_after = connection()->connection_sharing_allowed();

  event->end_time = std::chrono::steady_clock::now();
  auto &attrs = event->attrs;

  attrs.emplace_back("mysql.sharing_blocked", !allowed_after);

  if (!allowed_after) {
    // stringify why sharing is blocked.

    attrs.emplace_back("mysql.sharing_blocked_by",
                       connection()->connection_sharing_blocked_by());
  }

  trace_span_end(event, status_code);
}
