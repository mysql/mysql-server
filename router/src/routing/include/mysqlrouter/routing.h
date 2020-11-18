/*
  Copyright (c) 2015, 2020, Oracle and/or its affiliates. All rights reserved.

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

#include <chrono>
#include <map>
#include <string>

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

}  // namespace routing

#endif  // MYSQLROUTER_ROUTING_INCLUDED
