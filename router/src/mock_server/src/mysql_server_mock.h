/*
  Copyright (c) 2017, 2018, Oracle and/or its affiliates. All rights reserved.

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

#ifndef MYSQLD_MOCK_MYSQL_SERVER_MOCK_INCLUDED
#define MYSQLD_MOCK_MYSQL_SERVER_MOCK_INCLUDED

#include <memory>
#include <mutex>
#include <set>

#include "mysql/harness/plugin.h"
#include "mysql_protocol_decoder.h"
#include "mysql_protocol_encoder.h"
#include "mysqlrouter/mock_server_component.h"
#include "statement_reader.h"

namespace server_mock {

class MySQLServerMockSession {
 public:
  MySQLServerMockSession(
      socket_t client_sock,
      std::unique_ptr<StatementReaderBase> statement_processor,
      bool debug_mode);

  ~MySQLServerMockSession();

  void send_handshake(socket_t client_socket,
                      mysql_protocol::Capabilities::Flags our_capabilities);

  mysql_protocol::HandshakeResponsePacket handle_handshake_response(
      socket_t client_socket,
      mysql_protocol::Capabilities::Flags our_capabilities);

  void handle_auth_switch(socket_t client_socket);

  void send_fast_auth(socket_t client_socket);

  bool process_statements(socket_t client_socket);

  void handle_statement(socket_t client_socket, uint8_t seq_no,
                        const StatementAndResponse &statement);

  void send_error(socket_t client_socket, uint8_t seq_no, uint16_t error_code,
                  const std::string &error_msg,
                  const std::string &sql_state = "HY000");

  void send_ok(socket_t client_socket, uint8_t seq_no,
               uint64_t affected_rows = 0, uint64_t last_insert_id = 0,
               uint16_t server_status = 0, uint16_t warning_count = 0);

  void run();

  void kill() { killed_ = true; }

 private:
  bool killed_{false};
  socket_t client_socket_;
  MySQLProtocolEncoder protocol_encoder_;
  MySQLProtocolDecoder protocol_decoder_;
  std::unique_ptr<StatementReaderBase> json_reader_;
  bool debug_mode_;
};

/** @class MySQLServerMock
 *
 * @brief Main class. Resposible for accepting and handling client's
 *connections.
 *
 **/
class MySQLServerMock {
 public:
  /** @brief Constructor.
   *
   * @param expected_queries_file Path to the json file with definitins
   *                        of the expected SQL statements and responses
   * @param module_prefix prefix of javascript modules used by the nodejs
   * compatible module-loader
   * @param bind_port Number of the port on which the server accepts clients
   *                        connections
   * @param debug_mode Flag indicating if the handled queries should be printed
   * to the standard output
   */
  MySQLServerMock(const std::string &expected_queries_file,
                  const std::string &module_prefix, unsigned bind_port,
                  bool debug_mode);

  /** @brief Starts handling the clients connections in infinite loop.
   *         Will return only in case of an exception (error).
   */
  void run(mysql_harness::PluginFuncEnv *env);

  std::shared_ptr<MockServerGlobalScope> get_global_scope() {
    return shared_globals_;
  }

  void close_all_connections();

  ~MySQLServerMock();

 private:
  void setup_service();

  void handle_connections(mysql_harness::PluginFuncEnv *env);

  static constexpr int kListenQueueSize = 128;
  unsigned bind_port_;
  bool debug_mode_;
  socket_t listener_{socket_t(-1)};
  std::string expected_queries_file_;
  std::string module_prefix_;

  std::shared_ptr<MockServerGlobalScope> shared_globals_{
      new MockServerGlobalScope};

  std::mutex active_fds_mutex_;
  std::set<socket_t> active_fds_;
};

}  // namespace server_mock

#endif  // MYSQLD_MOCK_MYSQL_SERVER_MOCK_INCLUDED
