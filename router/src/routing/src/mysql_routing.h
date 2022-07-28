/*
  Copyright (c) 2015, 2022, Oracle and/or its affiliates.

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

#ifndef ROUTING_MYSQLROUTING_INCLUDED
#define ROUTING_MYSQLROUTING_INCLUDED

#include "mysqlrouter/routing_export.h"
#include "mysqlrouter/routing_plugin_export.h"

/** @file
 * @brief Defining the class MySQLRouting
 *
 * This file defines the main class `MySQLRouting` which is used to configure,
 * start and manage a connection routing from clients and MySQL servers.
 *
 */

#include <array>
#include <atomic>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <stdexcept>

#ifndef _WIN32
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>
#else
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

#include "connection.h"
#include "connection_container.h"
#include "context.h"
#include "destination.h"
#include "destination_ssl_context.h"
#include "mysql/harness/filesystem.h"
#include "mysql/harness/net_ts/internet.h"
#include "mysql/harness/net_ts/io_context.h"
#include "mysql/harness/net_ts/local.h"
#include "mysql/harness/plugin.h"
#include "mysql/harness/stdx/monitor.h"
#include "mysql_router_thread.h"
#include "mysql_routing_base.h"
#include "mysqlrouter/routing.h"
#include "mysqlrouter/routing_component.h"
#include "mysqlrouter/routing_export.h"
#include "mysqlrouter/uri.h"
#include "plugin_config.h"
#include "protocol/base_protocol.h"
#include "socket_container.h"
#include "ssl_mode.h"
#include "tcp_address.h"

namespace mysql_harness {
class PluginFuncEnv;
}

struct Nothing {};

/** @class MySQLRouting
 *  @brief Manage Connections from clients to MySQL servers
 *
 *  The class MySQLRouter is used to start a service listening on a particular
 *  TCP port for incoming MySQL Client connection and route these to a MySQL
 *  Server.
 *
 *  Connection routing will not analyze or parse any MySQL package (except from
 *  those in the handshake phase to be able to discover invalid connection
 * error) nor will it do any authentication. It will not handle errors from the
 * MySQL server and not automatically recover. The client communicate through
 *  MySQL Router just like it would directly connecting.
 *
 *  The MySQL Server is chosen from a given list of hosts or IP addresses
 *  (with or without TCP port) based on the the mode. For example, mode
 *  read-only will go through the list of servers in a round-robin way. The
 *  mode read-write will always go through the list from the beginning and
 *  failover to the next available.
 *
 *
 *  Example usage: bind to all IP addresses and use TCP Port 7001
 *
 *   MySQLRouting r(routing::AccessMode::kReadWrite, "0.0.0.0", 7001);
 *   r.destination_connect_timeout = std::chrono::seconds(1);
 *   r.set_destinations_from_csv("10.0.10.5;10.0.11.6");
 *   r.start();
 *
 *  The above example will, when MySQL running on 10.0.10.5 is not available,
 *  use 10.0.11.6 to setup the connection routing.
 *
 */
class ROUTING_EXPORT MySQLRouting : public MySQLRoutingBase {
 public:
  /** @brief Default constructor
   *
   * @param io_ctx IO context
   * @param routing_strategy routing strategy
   * @param port TCP port for listening for incoming connections
   * @param protocol protocol for the routing
   * @param access_mode access mode of the servers
   * @param bind_address bind_address Bind to particular IP address
   * @param named_socket Bind to Unix socket/Windows named pipe
   * @param route_name Name of connection routing (can be empty string)
   * @param max_connections Maximum allowed active connections
   * @param destination_connect_timeout Timeout trying to connect destination
   * server
   * @param max_connect_errors Maximum connect or handshake errors per host
   * @param connect_timeout Timeout waiting for handshake response
   * @param net_buffer_length send/receive buffer size
   * @param client_ssl_mode SSL mode of the client side
   * @param client_ssl_ctx SSL context of the client side
   * @param server_ssl_mode SSL mode of the serer side
   * @param dest_ssl_ctx SSL contexts of the destinations
   */
  MySQLRouting(
      net::io_context &io_ctx, routing::RoutingStrategy routing_strategy,
      uint16_t port, const Protocol::Type protocol,
      const routing::AccessMode access_mode = routing::AccessMode::kUndefined,
      const std::string &bind_address = {"0.0.0.0"},
      const mysql_harness::Path &named_socket = mysql_harness::Path(),
      const std::string &route_name = {},
      int max_connections = routing::kDefaultMaxConnections,
      std::chrono::milliseconds destination_connect_timeout =
          routing::kDefaultDestinationConnectionTimeout,
      unsigned long long max_connect_errors = routing::kDefaultMaxConnectErrors,
      std::chrono::milliseconds connect_timeout =
          routing::kDefaultClientConnectTimeout,
      unsigned int net_buffer_length = routing::kDefaultNetBufferLength,
      SslMode client_ssl_mode = SslMode::kDisabled,
      TlsServerContext *client_ssl_ctx = nullptr,
      SslMode server_ssl_mode = SslMode::kDisabled,
      DestinationTlsContext *dest_ssl_ctx = nullptr);

  /** @brief Starts the service and accept incoming connections
   *
   * Starts the connection routing service and start accepting incoming
   * MySQL client connections. Each connection will be further handled
   * in a separate thread.
   *
   * @throw std::runtime_error on errors.
   *
   */
  void start(mysql_harness::PluginFuncEnv *env);

  /** @brief Sets the destinations from URI
   *
   * Sets destinations using the given string and the given mode. The string
   * should be a comma separated list of MySQL servers.
   *
   * The mode is one of MySQLRouting::Mode, for example
   * MySQLRouting::Mode::kReadOnly.
   *
   * Example of destinations:
   *   "10.0.10.5,10.0.11.6:3307"
   *
   * @param csv destinations as comma-separated-values
   */
  void set_destinations_from_csv(const std::string &csv);

  void set_destinations_from_uri(const mysqlrouter::URI &uri);

  /** @brief Returns timeout when connecting to destination
   *
   * @return Timeout in seconds as int
   */
  std::chrono::milliseconds get_destination_connect_timeout() const noexcept {
    return context_.get_destination_connect_timeout();
  }

  /** @brief Sets timeout when connecting to destination
   *
   * Checks timeout connecting with destination servers.
   *
   * @throw std::invalid_argument when an invalid value was provided.
   *
   * @param timeout Timeout
   */
  void validate_destination_connect_timeout(std::chrono::milliseconds timeout);

  /** @brief Sets maximum active connections
   *
   * Sets maximum of active connections. Maximum must be between 1 and
   * 65535.
   *
   * @throw std::invalid_argument when an invalid value was provided.
   *
   * @param maximum Max number of connections allowed
   * @return New value as int
   */
  int set_max_connections(int maximum);

  /** @brief Returns maximum active connections
   *
   * @return Maximum as int
   */
  int get_max_connections() const noexcept override { return max_connections_; }

  /**
   * create new connection to MySQL Server than can handle client's
   * traffic and adds it to connection container.
   *
   * @param client_socket socket used to transfer data to/from client
   * @param client_endpoint endpoint of client
   */
  template <class ClientProtocol>
  void create_connection(
      typename ClientProtocol::socket client_socket,
      const typename ClientProtocol::endpoint &client_endpoint);

  routing::RoutingStrategy get_routing_strategy() const override;

  routing::AccessMode get_mode() const override;

  std::vector<mysql_harness::TCPAddress> get_destinations() const override;

  std::vector<MySQLRoutingAPI::ConnData> get_connections() override;

  RouteDestination *destinations() { return destination_.get(); }

  net::ip::tcp::acceptor &tcp_socket() { return service_tcp_; }

  void disconnect_all();

  /**
   * Stop accepting new connections on a listening socket.
   */
  void stop_socket_acceptors() override;

  /**
   * Check if we are accepting connections on a routing socket.
   *
   * @retval true if we are accepting connections, false otherwise
   */
  bool is_accepting_connections() const override;

  /**
   * Start accepting new connections on a listening socket
   *
   * @returns std::error_code on errors.
   */
  stdx::expected<void, std::error_code> start_accepting_connections() override;

 private:
  /**
   * Get listening socket detail information used for the logging purposes.
   */
  std::string get_port_str() const;

  /** @brief Sets up the TCP service
   *
   * Sets up the TCP service binding to IP addresses and TCP port.
   *
   * @returns std::error_code on errors.
   */
  stdx::expected<void, std::error_code> setup_tcp_service();

  /** @brief Sets up the named socket service
   *
   * Sets up the named socket service creating a socket file on UNIX systems.
   *
   * @returns std::error_code on errors.
   */
  stdx::expected<void, std::error_code> setup_named_socket_service();

  /** @brief Sets unix socket permissions so that the socket is accessible
   *         to all users (no-op on Windows)
   * @param socket_file path to socket file
   *
   * @throw std::runtime_error if chmod() inside fails
   */
  static void set_unix_socket_permissions(const char *socket_file);

  stdx::expected<void, std::error_code> start_acceptor(
      mysql_harness::PluginFuncEnv *env);

 public:
  MySQLRoutingContext &get_context() override { return context_; }

  bool is_running() const override { return is_running_; }

 private:
  /** Monitor for notifying socket acceptor */
  WaitableMonitor<Nothing> acceptor_waitable_{Nothing{}};

  /** @brief wrapper for data used by all connections */
  MySQLRoutingContext context_;

  net::io_context &io_ctx_;

  /** @brief Destination object to use when getting next connection */
  std::unique_ptr<RouteDestination> destination_;

  bool is_destination_standalone_{false};

  /** @brief Routing strategy to use when getting next destination */
  routing::RoutingStrategy routing_strategy_;

  /** @brief Access mode of the servers in the routing */
  routing::AccessMode access_mode_;

  /** @brief Maximum active connections
   *
   * Maximum number of incoming connections that will be accepted
   * by this MySQLRouter instances. There is no maximum for outgoing
   * connections since it is one-to-one with incoming.
   */
  int max_connections_;

  /** @brief Socket descriptor of the TCP service */
  net::ip::tcp::acceptor service_tcp_;
  net::ip::tcp::endpoint service_tcp_endpoint_;

#if !defined(_WIN32)
  /** @brief Socket descriptor of the named socket service */
  local::stream_protocol::acceptor service_named_socket_;
  local::stream_protocol::endpoint service_named_endpoint_;
#endif

  /** @brief used to unregister from subscription on allowed nodes changes */
  AllowedNodesChangeCallbacksListIterator allowed_nodes_list_iterator_;

  /** @brief container for connections */
  ConnectionContainer connection_container_;

  /** Information if the routing plugging is still running. */
  std::atomic<bool> is_running_{true};

#ifdef FRIEND_TEST
  FRIEND_TEST(RoutingTests, bug_24841281);
  FRIEND_TEST(RoutingTests, get_routing_thread_name);
  FRIEND_TEST(ClassicProtocolRoutingTest, NoValidDestinations);
  FRIEND_TEST(TestSetupTcpService, single_addr_ok);
  FRIEND_TEST(TestSetupTcpService, getaddrinfo_fails);
  FRIEND_TEST(TestSetupTcpService, socket_fails_for_all_addr);
  FRIEND_TEST(TestSetupTcpService, socket_fails);
  FRIEND_TEST(TestSetupTcpService, bind_fails);
  FRIEND_TEST(TestSetupTcpService, listen_fails);
#ifndef _WIN32
  FRIEND_TEST(TestSetupTcpService, setsockopt_fails);
  FRIEND_TEST(TestSetupNamedSocketService, unix_socket_permissions_failure);
#endif
#endif
};

extern "C" {
extern mysql_harness::Plugin ROUTING_PLUGIN_EXPORT harness_plugin_routing;
}

#endif  // ROUTING_MYSQLROUTING_INCLUDED
