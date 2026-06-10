/*
  Copyright (c) 2015, 2026, Oracle and/or its affiliates.

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

#ifndef ROUTING_MYSQLROUTING_INCLUDED
#define ROUTING_MYSQLROUTING_INCLUDED

#include "mysqlrouter/datatypes.h"
#include "mysqlrouter/routing_export.h"
#include "mysqlrouter/routing_plugin_export.h"

/** @file
 * @brief Defining the class MySQLRouting
 *
 * This file defines the main class `MySQLRouting` which is used to configure,
 * start and manage a connection routing from clients and MySQL servers.
 *
 */

#include <atomic>
#include <chrono>
#include <memory>

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
#include "mysqlrouter/base_protocol.h"
#include "mysqlrouter/io_thread.h"
#include "mysqlrouter/metadata_cache_datatypes.h"
#include "mysqlrouter/routing.h"
#include "mysqlrouter/routing_component.h"
#include "mysqlrouter/uri.h"
#include "plugin_config.h"
#include "routing_guidelines/routing_guidelines.h"
#include "socket_container.h"

namespace mysql_harness {
class PluginFuncEnv;
}  // namespace mysql_harness

struct Nothing {};
class MySQLRouting;

class ROUTING_EXPORT AcceptingEndpoint {
 public:
  AcceptingEndpoint(net::io_context &io_ctx,
                    const std::string &parent_routing_name)
      : io_ctx_(io_ctx), parent_routing_name_(parent_routing_name) {}

  AcceptingEndpoint(const AcceptingEndpoint &) = delete;

  virtual stdx::expected<void, std::error_code> setup() = 0;
  virtual stdx::expected<void, std::error_code> cancel() = 0;
  virtual bool is_open() const = 0;

  virtual void start(MySQLRouting *r, std::list<IoThread> &,
                     WaitableMonitor<Nothing> &waitable) = 0;

  virtual std::string name() = 0;

  virtual ~AcceptingEndpoint() {}

 protected:
  net::io_context &io_ctx_;
  // used when the acceptor logs
  std::string parent_routing_name_;
};

class ROUTING_EXPORT AcceptingEndpointTcpSocket : public AcceptingEndpoint {
 public:
  AcceptingEndpointTcpSocket(net::io_context &io_ctx,
                             const std::string &parent_routing_name,
                             const std::string &address, uint16_t port);

  stdx::expected<void, std::error_code> setup() override;
  stdx::expected<void, std::error_code> cancel() override;
  bool is_open() const override;

  void start(MySQLRouting *r, std::list<IoThread> &,
             WaitableMonitor<Nothing> &waitable) override;

  std::string name() override;

 private:
  net::ip::tcp::acceptor service_;
  net::ip::tcp::endpoint service_endpoint_;

  std::string address_;
  uint16_t port_;
};

#ifndef _WIN32
class ROUTING_EXPORT AcceptingEndpointUnixSocket : public AcceptingEndpoint {
 public:
  AcceptingEndpointUnixSocket(net::io_context &io_ctx,
                              const std::string &parent_routing_name,
                              const std::string &socket_name);

  stdx::expected<void, std::error_code> setup() override;
  stdx::expected<void, std::error_code> cancel() override;
  bool is_open() const override;

  void start(MySQLRouting *r, std::list<IoThread> &,
             WaitableMonitor<Nothing> &waitable) override;

  std::string name() override;

 private:
  local::stream_protocol::acceptor service_;
  local::stream_protocol::endpoint service_endpoint_;

  std::string socket_name_;
};
#endif

/** @class MySQLRouting
 *  @brief Manage Connections from clients to MySQL servers
 *
 *  The class MySQLRouter is used to start a service listening on a particular
 *  TCP port for incoming MySQL Client connection and route these to a MySQL
 *  Server.
 */
class ROUTING_EXPORT MySQLRouting : public MySQLRoutingBase {
 public:
  /** @brief Default constructor
   *
   * @param routing_config routing configuration
   * @param io_ctx IO context
   * @param guidelines routing guidelines engine
   * @param route_name Name of connection routing (can be empty string)
   * @param client_ssl_ctx SSL context of the client side
   * @param dest_ssl_ctx SSL contexts of the destinations
   */
  MySQLRouting(
      const RoutingConfig &routing_config, net::io_context &io_ctx,
      std::shared_ptr<routing_guidelines::Routing_guidelines_engine> guidelines,
      const std::string &route_name = {},
      TlsServerContext *client_ssl_ctx = nullptr,
      DestinationTlsContext *dest_ssl_ctx = nullptr);

  /** @brief Runs the service and accept incoming connections
   *
   * Runs the connection routing service and starts accepting incoming
   * MySQL client connections.
   *
   * @throw std::runtime_error on errors.
   *
   */
  void run(mysql_harness::PluginFuncEnv *env);

  void set_destinations(const std::string &dests);

  /**
   * Sets the destinations.
   *
   * @param dests destinations
   */
  void set_destinations_from_dests(
      const std::vector<mysql_harness::Destination> &dests);

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

  std::optional<routing::RoutingStrategy> get_routing_strategy()
      const override {
    return routing_strategy_;
  }

  std::vector<mysql_harness::Destination> get_destination_candidates()
      const override;

  std::vector<MySQLRoutingAPI::ConnData> get_connections() override;

  MySQLRoutingConnectionBase *get_connection(const std::string &) override;

  DestinationManager *destination_manager() override {
    return destination_manager_.get();
  }

  void disconnect_all();

  /**
   * Stop accepting new connections on a listening socket.
   *
   * @param shutting_down is plugin shutting down.
   */
  void stop_socket_acceptors(const bool shutting_down) override;

  /**
   * Check if we are accepting connections on a routing socket.
   *
   * @retval true if we are accepting connections, false otherwise
   */
  bool is_accepting_connections() const override;

  /**
   * Start accepting new connections on a listening socket
   *
   * @returns std::string on errors.
   */
  stdx::expected<void, std::string> start_accepting_connections() override;

  /**
   * Start accepting new connections on a listening socket after it has been
   * quarantined for lack of valid destinations
   *
   * @returns std::string on errors.
   */
  stdx::expected<void, std::string> restart_accepting_connections() override;

  /**
   * In case when routing guideline was updated go through each established
   * connection and verify if it is allowed according to the new guideline. If
   * not then such connection is dropped.
   *
   * @param affected_routing_sources list of routing guideline route names that
   * were affected by the guideline update
   */
  void on_routing_guidelines_update(
      const routing_guidelines::Routing_guidelines_engine::RouteChanges
          &affected_routing_sources);

  /**
   * Try to update routing guideline with a new guideline.
   *
   * @return list of routing guideline route names that were affected by the
   * guideline update
   */
  routing_guidelines::Routing_guidelines_engine::RouteChanges
  update_routing_guidelines(const std::string &routing_guidelines_document);

  /**
   * If the router info was updated then register this info in routing context.
   *
   * @param router_info updated router info
   */
  void on_router_info_update(
      const routing_guidelines::Router_info &router_info);

  bool is_standalone() const override { return is_destination_standalone_; }

 private:
  /** @brief Sets unix socket permissions so that the socket is accessible
   *         to all users (no-op on Windows)
   * @param socket_file path to socket file
   *
   * @throw std::runtime_error if chmod() inside fails
   */
  static void set_unix_socket_permissions(const char *socket_file);

  stdx::expected<void, std::string> run_acceptor(
      mysql_harness::PluginFuncEnv *env);

  stdx::expected<void, std::string> run_with_no_acceptor(
      mysql_harness::PluginFuncEnv *env);

 public:
  MySQLRoutingContext &get_context() override { return context_; }

  bool is_running() const override { return is_running_; }

  /**
   * get the purpose of connections to this route.
   *
   * - read-write : all statements are treated as "read-write".
   * - read-only  : all statements are treated as "read-only".
   * - unavailable: it is currently unknown where the statement should go to.
   *
   * "Unavailable" is used for read-write splitting where the purpose is
   * determined per statement, session, ...
   *
   * A statement over a read-only server connection may end up on a read-write
   * server in case all read-only servers aren't reachable. Even if the server
   * is read-write, the connections purpose is read-only and if the server
   * changes its role from PRIMARY to SECONDARY, these read-only connections
   * will not be abort as a SECONDARY is good enough to serve read-only
   * connections.
   */
  mysqlrouter::ServerMode purpose() const override;

 private:
  bool accept_connections_{true};

  /** Monitor for notifying socket acceptor */
  WaitableMonitor<Nothing> acceptor_waitable_{Nothing{}};

  /** @brief wrapper for data used by all connections */
  MySQLRoutingContext context_;

  net::io_context &io_ctx_;

  /** @brief Destination object to use when getting next connection */
  std::unique_ptr<DestinationManager> destination_manager_{nullptr};

  bool is_destination_standalone_{false};

  /** @brief Routing strategy to use when getting next destination */
  std::optional<routing::RoutingStrategy> routing_strategy_;

  /** @brief access_mode of the servers in the routing */
  routing::AccessMode access_mode_;

  /** @brief Maximum active connections
   *
   * Maximum number of incoming connections that will be accepted
   * by this MySQLRouter instances. There is no maximum for outgoing
   * connections since it is one-to-one with incoming.
   */
  int max_connections_;

  /** @brief used to unregister from subscription on allowed nodes changes */
  AllowedNodesChangeCallbacksListIterator allowed_nodes_list_iterator_;

  /** @brief container for connections */
  ConnectionContainer connection_container_;

  /** Information if the routing plugin is still running. */
  std::atomic<bool> is_running_{true};

  /** Used when the accepting port is been reopened and it failed, to schedule
   * another retry for standalone-destination(s) route. */
  net::steady_timer accept_port_reopen_retry_timer_{io_ctx_};

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

  std::vector<std::unique_ptr<AcceptingEndpoint>> accepting_endpoints_;
};

extern "C" {
extern mysql_harness::Plugin ROUTING_PLUGIN_EXPORT harness_plugin_routing;
}

#endif  // ROUTING_MYSQLROUTING_INCLUDED
