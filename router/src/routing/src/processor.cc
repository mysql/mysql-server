/*
  Copyright (c) 2022, 2023, Oracle and/or its affiliates.

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

#include "processor.h"

#include "classic_connection_base.h"
#include "mysql/harness/logging/logging.h"
#include "mysql/harness/stdx/expected.h"
#include "mysql/harness/tls_error.h"

IMPORT_LOG_FUNCTIONS()

stdx::expected<Processor::Result, std::error_code>
Processor::send_server_failed(std::error_code ec) {
  // don't call finish() as the calling loop() will call it for us when the
  // error is returned.
  connection()->send_server_failed(ec, false);

  return stdx::make_unexpected(ec);
}

stdx::expected<Processor::Result, std::error_code>
Processor::recv_server_failed(std::error_code ec) {
  if (ec == TlsErrc::kWantRead) return Result::RecvFromServer;

  // don't call finish() as the calling loop() will call it for us when the
  // error is returned.
  connection()->recv_server_failed(ec, false);

  return stdx::make_unexpected(ec);
}

stdx::expected<Processor::Result, std::error_code>
Processor::send_client_failed(std::error_code ec) {
  // don't call finish() as the calling loop() will call it for us when the
  // error is returned.
  connection()->send_client_failed(ec, false);

  return stdx::make_unexpected(ec);
}

stdx::expected<Processor::Result, std::error_code>
Processor::recv_client_failed(std::error_code ec) {
  if (ec == TlsErrc::kWantRead) return Result::RecvFromClient;

  // don't call finish() as the calling loop() will call it for us when the
  // error is returned.
  connection()->recv_client_failed(ec, false);

  return stdx::make_unexpected(ec);
}

stdx::expected<Processor::Result, std::error_code>
Processor::server_socket_failed(std::error_code ec) {
  // don't call finish() as the calling loop() will call it for us when the
  // error is returned.
  connection()->server_socket_failed(ec, false);

  return stdx::make_unexpected(ec);
}

stdx::expected<Processor::Result, std::error_code>
Processor::client_socket_failed(std::error_code ec) {
  // don't call finish() as the calling loop() will call it for us when the
  // error is returned.
  connection()->client_socket_failed(ec, false);

  return stdx::make_unexpected(ec);
}

stdx::expected<void, std::error_code> Processor::discard_current_msg(
    Channel *src_channel, ClassicProtocolState *src_protocol) {
  auto &opt_current_frame = src_protocol->current_frame();
  if (!opt_current_frame) return {};

  auto &current_frame = *opt_current_frame;

  auto &recv_buf = src_channel->recv_plain_view();

  if (recv_buf.size() < current_frame.frame_size_) {
    // received message is incomplete.
    return stdx::make_unexpected(make_error_code(std::errc::bad_message));
  }
  if (current_frame.forwarded_frame_size_ != 0) {
    // partially forwarded already.
    return stdx::make_unexpected(make_error_code(std::errc::invalid_argument));
  }

  src_channel->consume_plain(current_frame.frame_size_);

  // unset current frame and also current-msg
  src_protocol->current_frame().reset();
  src_protocol->current_msg_type().reset();

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
