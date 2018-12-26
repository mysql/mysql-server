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

#ifndef MYSQLROUTER_ROUTING_INCLUDED
#define MYSQLROUTER_ROUTING_INCLUDED

#include "mysqlrouter/plugin_config.h"
#include "socket_operations.h"
#include "tcp_address.h"

#include <map>
#include <string>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <poll.h>
#endif

#ifdef _WIN32
typedef ULONG nfds_t;
typedef long ssize_t;
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

namespace routing {

/** @brief Timeout for idling clients (in seconds)
 *
 * Constant defining how long (in seconds) a client can keep the connection
 * idling. This is similar to the wait_timeout variable in the MySQL Server.
 */
extern const int kDefaultWaitTimeout;

/** @brief Max number of active routes for this routing instance */
extern const int kDefaultMaxConnections;

/** @brief Timeout connecting to destination (in seconds)
 *
 * Constant defining how long we wait to establish connection with the server
 * before we give up.
 */
extern const std::chrono::seconds kDefaultDestinationConnectionTimeout;

/** @brief Maximum connect or handshake errors per host
 *
 * Maximum connect or handshake errors after which a host will be
 * blocked. Such errors can happen when the client does not reply
 * the handshake, sends an incorrect packet, or garbage.
 *
 */
extern const unsigned long long kDefaultMaxConnectErrors;

/** @brief Maximum connect or handshake errors per host
 *
 * Maximum connect or handshake errors after which a host will be
 * blocked. Such errors can happen when the client does not reply
 * the handshake, sends an incorrect packet, or garbage.
 *
 */
extern const unsigned long long kDefaultMaxConnectErrors;

/** @brief Default bind address
 *
 */
extern const std::string kDefaultBindAddress;

/** @brief Default net buffer length
 *
 * Default network buffer length which can be set in the MySQL Server.
 *
 * This should match the default of the latest MySQL Server.
 */
extern const unsigned int kDefaultNetBufferLength;

/** @brief Timeout waiting for handshake response from client
 *
 * The number of seconds that MySQL Router waits for a handshake response.
 * The default value is 9 seconds (default MySQL Server minus 1).
 *
 */
extern const std::chrono::seconds kDefaultClientConnectTimeout;

#ifdef _WIN32
const SOCKET kInvalidSocket =
    INVALID_SOCKET;  // windows defines INVALID_SOCKET already
#else
const int kInvalidSocket = -1;
#endif

/** @brief Modes supported by Routing plugin */
enum class AccessMode {
  kUndefined = 0,
  kReadWrite = 1,
  kReadOnly = 2,
};

/** @brief Routing strategies supported by Routing plugin */
enum class RoutingStrategy {
  kUndefined = 0,
  kFirstAvailable = 1,
  kNextAvailable = 2,
  kRoundRobin = 3,
  kRoundRobinWithFallback = 4,
};

/** @brief Get comma separated list of all access mode names
 *
 */
std::string get_access_mode_names();

/** @brief Returns AccessMode for its literal representation
 *
 * If no AccessMode is found for given string,
 * AccessMode::kUndefined is returned.
 *
 * @param value literal representation of the access mode
 * @return AccessMode for the given string or AccessMode::kUndefined
 */
AccessMode get_access_mode(const std::string &value);

/** @brief Returns literal name of given access mode
 *
 * Returns literal name of given access mode as a std:string. When
 * the access mode is not found, empty string is returned.
 *
 * @param access_mode Access mode to look up
 * @return Name of access mode as std::string or empty string
 */
std::string get_access_mode_name(AccessMode access_mode) noexcept;

/** @brief Get comma separated list of all routing stategy names
 *         for a given routing type (metadata cache or static)
 *
 *
 * @param metadata_cache bool flag indicating if the list should contain
 *                       strategies supported for metadata_cache
 *                        or static routing
 */
std::string get_routing_strategy_names(bool metadata_cache);

/** @brief Returns RoutingStrategy for its literal representation
 *
 * If no RoutingStrategy is found for given string,
 * RoutingStrategy::kUndefined is returned.
 *
 * @param value literal representation of the access mode
 * @return RoutingStrategy for the given string or RoutingStrategy::kUndefined
 */
RoutingStrategy get_routing_strategy(const std::string &value);

/** @brief Returns literal name of given routing strategy
 *
 * Returns literal name of given routing strategy as a std:string. When
 * the routing strategy is not found, empty string is returned.
 *
 * @param routing_strategy Routing strategy to look up
 * @return Name of routing strategy as std::string or empty string
 */
std::string get_routing_strategy_name(
    RoutingStrategy routing_strategy) noexcept;

/**
 * Sets blocking flag for given socket
 *
 * @param sock a socket file descriptor
 * @param blocking whether to set blocking off (false) or on (true)
 */
void set_socket_blocking(int sock, bool blocking);

/** @class RoutingSockOpsInterface
 * @brief Interface class to allow multiple RoutingSockOps implementations
 *        (at least one "real" and one mock for testing purposes)
 */
class RoutingSockOpsInterface {
 public:
  virtual ~RoutingSockOpsInterface() = default;
  virtual int get_mysql_socket(mysql_harness::TCPAddress addr,
                               std::chrono::milliseconds connect_timeout_ms,
                               bool log = true) noexcept = 0;
  virtual mysql_harness::SocketOperationsBase *so() const = 0;
};

/** @class RoutingSockOps
 * @brief This class provides a "real" (not mock) implementation
 */
class RoutingSockOps : public RoutingSockOpsInterface {
 public:
  RoutingSockOps(mysql_harness::SocketOperationsBase *sock_ops)
      : so_(sock_ops) {}

  static RoutingSockOps *instance(
      mysql_harness::SocketOperationsBase *sock_ops);

  /** @brief Returns socket descriptor of connected MySQL server
   *
   * Iterates through all available connections (until it succesfully connects)
   * to the selected address as returned by getaddrinfo()
   * (see its documentation for the details).
   * If it's not able to connect via any path, it returns value < 0.
   *
   * Returns a socket descriptor for the connection to the MySQL Server or
   * negative value when error occurred:
   *  -2 - if connection timeout has expired for at least one of the attempted
   * paths -1 - in case of any other error
   *
   * @param addr information of the server we connect with
   * @param connect_timeout timeout waiting for connection
   * @param log whether to log errors or not
   * @return a socket descriptor
   */
  int get_mysql_socket(mysql_harness::TCPAddress addr,
                       std::chrono::milliseconds connect_timeout,
                       bool log = true) noexcept override;

  /** @brief Returns SocketOperations implementation used by this class */
  mysql_harness::SocketOperationsBase *so() const override { return so_; }

 private:
  RoutingSockOps() = default;
  RoutingSockOps(const RoutingSockOps &) = delete;
  RoutingSockOps operator=(const RoutingSockOps &) = delete;

  mysql_harness::SocketOperationsBase *so_;
};

}  // namespace routing

#endif  // MYSQLROUTER_ROUTING_INCLUDED
