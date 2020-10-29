/*
  Copyright (c) 2019, 2020, Oracle and/or its affiliates.

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

#include "mock_session.h"

#include <array>
#include <chrono>
#include <iostream>  // cout
#include <memory>    // unique_ptr
#include <system_error>
#include <thread>  // sleep_for

#include "classic_mock_session.h"
#include "harness_assert.h"
#include "mysql/harness/logging/logging.h"  // log_
#include "mysql/harness/net_ts/internet.h"  // net::ip::tcp
#include "mysql/harness/net_ts/socket.h"
#include "mysql/harness/stdx/expected.h"
#include "x_mock_session.h"

IMPORT_LOG_FUNCTIONS()

using namespace std::chrono_literals;

namespace server_mock {

ProtocolBase::ProtocolBase(net::ip::tcp::socket &&client_sock,
                           net::impl::socket::native_handle_type wakeup_fd)
    : client_socket_{std::move(client_sock)}, wakeup_fd_{wakeup_fd} {
  // if it doesn't work, no problem.
  //
  client_socket_.set_option(net::ip::tcp::no_delay{true});
  client_socket_.native_non_blocking(true);
}

// check if the current socket is readable/open
stdx::expected<bool, std::error_code> ProtocolBase::socket_has_data(
    std::chrono::milliseconds timeout) {
  std::array<pollfd, 2> fds = {{
      {client_socket_.native_handle(), POLLIN, 0},
      {wakeup_fd_, POLLIN, 0},
  }};

  const auto poll_res = net::impl::poll::poll(fds.data(), fds.size(), timeout);
  if (!poll_res) {
    if (poll_res.error() == std::errc::timed_out) {
      return false;
    }

    return stdx::make_unexpected(poll_res.error());
  }

  if (fds[0].revents != 0) {
    return true;
  }

  // looks like the wakeup_fd fired
  return stdx::make_unexpected(make_error_code(std::errc::operation_canceled));
}

void ProtocolBase::send_buffer(net::const_buffer buf) {
  while (buf.size() > 0) {
    auto send_res = client_socket_.send(buf);
    if (!send_res) {
      if (send_res.error() == std::errc::operation_would_block) {
        auto writable_res = socket_has_data(-1ms);

        if (writable_res) continue;

        throw std::system_error(writable_res.error(), "send_buffer()");
      }
      throw std::system_error(send_res.error(), "send_buffer()");
    }

    buf += send_res.value();
  }
}

void ProtocolBase::read_buffer(net::mutable_buffer &buf) {
  while (buf.size() > 0) {
    auto recv_res = client_socket_.receive(buf);
    if (!recv_res) {
      if (recv_res.error() == std::errc::operation_would_block) {
        auto readable_res = socket_has_data(std::chrono::milliseconds{-1});
        if (readable_res) continue;

        throw std::system_error(readable_res.error(), "read_buffer()");
      }

      throw std::system_error(recv_res.error(), "read_buffer()");
    }

    buf += recv_res.value();
  }
}

static void debug_trace_result(const ResultsetResponse *resultset) {
  std::cout << "QUERY RESULT:\n";
  for (auto const &row : resultset->rows) {
    for (const auto &cell : row)
      std::cout << "  |  " << (cell ? cell.value() : "NULL");
    std::cout << "  |\n";
  }
  std::cout << "\n\n\n" << std::flush;
}

MySQLServerMockSession::MySQLServerMockSession(
    ProtocolBase *protocol,
    std::unique_ptr<StatementReaderBase> statement_processor, bool debug_mode)
    : json_reader_{std::move(statement_processor)},
      protocol_{protocol},
      debug_mode_{debug_mode} {}

void MySQLServerMockSession::run() {
  try {
    bool res = process_handshake();
    if (!res) {
      std::cout << "Error processing handshake with client: "
                << protocol_->client_socket().native_handle() << std::endl;
    }

    res = process_statements();
    if (!res) {
      std::cout << "Error processing statements with client: "
                << protocol_->client_socket().native_handle() << std::endl;
    }
  } catch (const std::exception &e) {
    log_warning("Exception caught in connection loop: %s", e.what());
  }
}

void MySQLServerMockSession::handle_statement(
    const StatementResponse &statement) {
  using ResponseType = StatementResponse::ResponseType;

  switch (statement.response_type) {
    case ResponseType::OK: {
      if (debug_mode_) std::cout << std::endl;  // visual separator
      auto *response = dynamic_cast<OkResponse *>(statement.response.get());

      harness_assert(response);
      std::this_thread::sleep_for(statement.exec_time);
      protocol_->send_ok(0, response->last_insert_id, 0,
                         response->warning_count);
    } break;
    case ResponseType::RESULT: {
      auto *response =
          dynamic_cast<ResultsetResponse *>(statement.response.get());
      harness_assert(response);
      if (debug_mode_) {
        debug_trace_result(response);
      }

      protocol_->send_resultset(*response, statement.exec_time);
    } break;
    case ResponseType::ERROR: {
      if (debug_mode_) std::cout << std::endl;  // visual separator
      auto *response = dynamic_cast<ErrorResponse *>(statement.response.get());
      harness_assert(response);
      protocol_->send_error(response->code, response->msg);
    } break;
    default:;
      throw std::runtime_error("Unsupported command in handle_statement(): " +
                               std::to_string((int)statement.response_type));
  }
}

}  // namespace server_mock
