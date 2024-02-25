/*
  Copyright (c) 2017, 2023, Oracle and/or its affiliates.

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
#include "mysql.h"  // mysql_ssl_mode
#include "mysql/harness/net_ts/internet.h"
#include "mysql/harness/net_ts/io_context.h"
#include "mysql/harness/net_ts/local.h"
#include "mysql/harness/plugin.h"
#include "mysql/harness/stdx/monitor.h"
#include "mysql/harness/tls_server_context.h"
#include "mysqlrouter/mock_server_component.h"
#include "statement_reader.h"

namespace server_mock {

/** @class MySQLServerMock
 *
 * @brief Main class. Responsible for accepting and handling client's
 *connections.
 *
 **/
class MySQLServerMock {
 public:
  /** @brief Constructor.
   *
   * @param io_ctx IO context for network operations
   * @param expected_queries_file Path to the json file with definitins
   *                        of the expected SQL statements and responses
   * @param module_prefixes prefixes of javascript modules used by the nodejs
   * compatible module-loader
   * @param bind_address Address on which the server accepts client connections
   * @param bind_port Number of the port on which the server accepts clients
   *                        connections
   * @param protocol the protocol this mock instance speaks: "x" or "classic"
   * @param debug_mode Flag indicating if the handled queries should be printed
   * to the standard output
   * @param tls_server_ctx TLS Server Context
   * @param ssl_mode SSL mode
   */
  MySQLServerMock(net::io_context &io_ctx, std::string expected_queries_file,
                  std::vector<std::string> module_prefixes,
                  std::string bind_address, unsigned bind_port,
                  std::string protocol, bool debug_mode,
                  TlsServerContext &&tls_server_ctx, mysql_ssl_mode ssl_mode);

  /** @brief Starts handling the clients connections in infinite loop.
   *         Will return only in case of an exception (error).
   */
  void run(mysql_harness::PluginFuncEnv *env);

  /**
   * close all open connections.
   *
   * can be called from other threads.
   */
  void close_all_connections();

 private:
  std::string bind_address_;
  unsigned bind_port_;
  bool debug_mode_;
  net::io_context &io_ctx_;
  std::string expected_queries_file_;
  std::vector<std::string> module_prefixes_;
  std::string protocol_name_;

  TlsServerContext tls_server_ctx_;

  mysql_ssl_mode ssl_mode_;

  WaitableMonitor<std::list<std::unique_ptr<MySQLServerMockSession>>>
      client_sessions_{{}};
};

}  // namespace server_mock

#endif  // MYSQLD_MOCK_MYSQL_SERVER_MOCK_INCLUDED
