/*
  Copyright (c) 2015, 2018, Oracle and/or its affiliates. All rights reserved.

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

/** @file
 * @brief Defining the class MySQLRouting
 *
 * This file defines the main class `MySQLRouting` which is used to configure,
 * start and manage a connection routing from clients and MySQL servers.
 *
 */

#include "connection.h"
#include "connection_container.h"
#include "context.h"
#include "destination.h"
#include "mysql/harness/filesystem.h"
#include "mysql_router_thread.h"
#include "mysqlrouter/mysql_protocol.h"
#include "mysqlrouter/routing.h"
#include "plugin_config.h"
#include "protocol/base_protocol.h"
#include "router_config.h"
#include "tcp_address.h"
#include "utils.h"
namespace mysql_harness {
class PluginFuncEnv;
}

#include <array>
#include <atomic>
#include <iostream>
#include <map>
#include <memory>

#ifndef _WIN32
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>
#else
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

#ifdef _WIN32
#ifdef routing_DEFINE_STATIC
#define ROUTING_API
#else
#ifdef routing_EXPORTS
#define ROUTING_API __declspec(dllexport)
#else
#define ROUTING_API __declspec(dllimport)
#endif
#endif
#else
#define ROUTING_API
#endif

using mysqlrouter::URI;
using std::string;

/** @class MySQLRoutering
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
class MySQLRouting {
 public:
  /** @brief Default constructor
   *
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
   * @param routing_sock_ops object handling the operations on network sockets
   * @param thread_stack_size memory in kilobytes allocated for thread's stack
   */
  MySQLRouting(
      routing::RoutingStrategy routing_strategy, uint16_t port,
      const Protocol::Type protocol,
      const routing::AccessMode access_mode = routing::AccessMode::kUndefined,
      const string &bind_address = string{"0.0.0.0"},
      const mysql_harness::Path &named_socket = mysql_harness::Path(),
      const string &route_name = string{},
      int max_connections = routing::kDefaultMaxConnections,
      std::chrono::milliseconds destination_connect_timeout =
          routing::kDefaultDestinationConnectionTimeout,
      unsigned long long max_connect_errors = routing::kDefaultMaxConnectErrors,
      std::chrono::milliseconds connect_timeout =
          routing::kDefaultClientConnectTimeout,
      unsigned int net_buffer_length = routing::kDefaultNetBufferLength,
      routing::RoutingSockOpsInterface *routing_sock_ops =
          routing::RoutingSockOps::instance(
              mysql_harness::SocketOperations::instance()),
      size_t thread_stack_size = mysql_harness::kDefaultStackSizeInKiloBytes);

  ~MySQLRouting();

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
  int get_max_connections() const noexcept { return max_connections_; }

  /**
   * @brief create new connection to MySQL Server than can handle client's
   * traffic and adds it to connection container. Every connection runs in it's
   * own thread of execution.
   *
   * @param client_socket socket used to send/receive data to/from client
   * @param client_addr address of client
   */
  void create_connection(int client_socket,
                         const sockaddr_storage &client_addr);

 private:
  /** @brief Sets up the TCP service
   *
   * Sets up the TCP service binding to IP addresses and TCP port.
   *
   * @throw std::runtime_error on errors.
   */
  void setup_tcp_service();

  /** @brief Sets up the named socket service
   *
   * Sets up the named socket service creating a socket file on UNIX systems.
   *
   * @throw std::runtime_error on errors.
   */
  void setup_named_socket_service();

  /** @brief Sets unix socket permissions so that the socket is accessible
   *         to all users (no-op on Windows)
   * @param socket_file path to socket file
   *
   * @throw std::runtime_error if chmod() inside fails
   */
  static void set_unix_socket_permissions(const char *socket_file);

 public:
  MySQLRoutingContext &get_context() { return context_; }

 private:
  void start_acceptor(mysql_harness::PluginFuncEnv *env);

  /** @brief wrapper for data used by all connections */
  MySQLRoutingContext context_;

  /** @brief object handling the operations on network sockets */
  routing::RoutingSockOpsInterface *routing_sock_ops_;

  /** @brief Destination object to use when getting next connection */
  std::unique_ptr<RouteDestination> destination_;

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
  int service_tcp_;
  /** @brief Socket descriptor of the named socket service */
  int service_named_socket_;

  /** @brief used to unregister from subscription on allowed nodes changes */
  AllowedNodesChangeCallbacksListIterator allowed_nodes_list_iterator_;

  /** @brief container for connections */
  ConnectionContainer connection_container_;

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
extern mysql_harness::Plugin ROUTING_API harness_plugin_routing;
}

#endif  // ROUTING_MYSQLROUTING_INCLUDED
