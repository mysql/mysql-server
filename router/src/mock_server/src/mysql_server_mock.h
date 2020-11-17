/*
  Copyright (c) 2017, 2020, Oracle and/or its affiliates.

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

#include "mock_session.h"
#include "mysql/harness/net_ts/internet.h"
#include "mysql/harness/net_ts/local.h"
#include "mysql/harness/plugin.h"
#include "mysqlrouter/mock_server_component.h"
#include "statement_reader.h"

namespace server_mock {

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
   * @param bind_address Address on which the server accepts client connections
   * @param bind_port Number of the port on which the server accepts clients
   *                        connections
   * @param protocol the protocol this mock instance speaks: "x" or "classic"
   * @param debug_mode Flag indicating if the handled queries should be printed
   * to the standard output
   */
  MySQLServerMock(std::string expected_queries_file, std::string module_prefix,
                  std::string bind_address, unsigned bind_port,
                  std::string protocol, bool debug_mode);

  /** @brief Starts handling the clients connections in infinite loop.
   *         Will return only in case of an exception (error).
   */
  void run(mysql_harness::PluginFuncEnv *env);

  void close_all_connections();

 private:
  void setup_service();

  void handle_connections(mysql_harness::PluginFuncEnv *env);

  static constexpr int kListenQueueSize = 128;
  std::string bind_address_;
  unsigned bind_port_;
  bool debug_mode_;
  net::io_context io_ctx_;
  net::ip::tcp::acceptor listener_{io_ctx_};
  std::string expected_queries_file_;
  std::string module_prefix_;
  std::string protocol_name_;
#if defined(_WIN32)
  net::ip::tcp::socket
#else
  local::stream_protocol::socket
#endif
      wakeup_sock_send_{io_ctx_};
};

class MySQLServerSharedGlobals {
 public:
  static std::shared_ptr<MockServerGlobalScope> get() noexcept {
    std::lock_guard<std::mutex> lk(mtx_);
    if (!shared_globals_) shared_globals_.reset(new MockServerGlobalScope);
    return shared_globals_;
  }

 private:
  static std::mutex mtx_;
  static std::shared_ptr<MockServerGlobalScope> shared_globals_;
};

}  // namespace server_mock

#endif  // MYSQLD_MOCK_MYSQL_SERVER_MOCK_INCLUDED
