/*
  Copyright (c) 2019, Oracle and/or its affiliates. All rights reserved.

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

#include "classic_mock_session.h"
#include "x_mock_session.h"

#include <thread>

#ifndef _WIN32
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <signal.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>
#else
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

#include "mysql/harness/logging/logging.h"
IMPORT_LOG_FUNCTIONS()

namespace server_mock {

static void debug_trace_result(const ResultsetResponse *resultset) {
  std::cout << "QUERY RESULT:\n";
  for (size_t i = 0; i < resultset->rows.size(); ++i) {
    for (const auto &cell : resultset->rows[i])
      std::cout << "  |  " << (cell.first ? cell.second : "NULL");
    std::cout << "  |\n";
  }
  std::cout << "\n\n\n" << std::flush;
}

MySQLServerMockSession::MySQLServerMockSession(
    socket_t client_sock,
    std::unique_ptr<StatementReaderBase> statement_processor, bool debug_mode)
    : client_socket_{client_sock},
      json_reader_{std::move(statement_processor)},
      debug_mode_{debug_mode} {
  // if it doesn't work, no problem.
  int one = 1;
  setsockopt(client_socket_, IPPROTO_TCP, TCP_NODELAY,
             reinterpret_cast<const char *>(&one), sizeof(one));

  non_blocking(client_socket_, false);
}

MySQLServerMockSession::~MySQLServerMockSession() {}

void MySQLServerMockSession::run() {
  try {
    bool res = process_handshake();
    if (!res) {
      std::cout << "Error processing handshake with client: " << client_socket_
                << std::endl;
    }

    res = process_statements();
    if (!res) {
      std::cout << "Error processing statements with client: " << client_socket_
                << std::endl;
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
      OkResponse *response =
          dynamic_cast<OkResponse *>(statement.response.get());

      harness_assert(response);
      std::this_thread::sleep_for(statement.exec_time);
      send_ok(0, response->last_insert_id, 0, response->warning_count);
    } break;
    case ResponseType::RESULT: {
      ResultsetResponse *response =
          dynamic_cast<ResultsetResponse *>(statement.response.get());
      harness_assert(response);
      if (debug_mode_) {
        debug_trace_result(response);
      }

      send_resultset(*response, statement.exec_time);
    } break;
    case ResponseType::ERROR: {
      if (debug_mode_) std::cout << std::endl;  // visual separator
      ErrorResponse *response =
          dynamic_cast<ErrorResponse *>(statement.response.get());
      harness_assert(response);
      send_error(response->code, response->msg);
    } break;
    default:;
      throw std::runtime_error("Unsupported command in handle_statement(): " +
                               std::to_string((int)statement.response_type));
  }
}

/*static*/ std::unique_ptr<MySQLServerMockSession>
MySQLServerMockSession::create_session(
    const std::string &protocol, socket_t client_socket,
    std::unique_ptr<StatementReaderBase> statement_processor, bool debug_mode) {
  std::unique_ptr<MySQLServerMockSession> result;
  if (protocol == "classic") {
    result.reset(new MySQLServerMockSessionClassic(
        client_socket, std::move(statement_processor), debug_mode));
  } else if (protocol == "x") {
    result.reset(new MySQLServerMockSessionX(
        client_socket, std::move(statement_processor), debug_mode));
  } else {
    throw std::runtime_error("Unknown protocol: '" + protocol + "'");
  }

  return result;
}

}  // namespace server_mock
