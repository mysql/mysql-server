/*
  Copyright (c) 2015, 2024, Oracle and/or its affiliates.

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

#ifndef MYSQLROUTER_ROUTING_INCLUDED
#define MYSQLROUTER_ROUTING_INCLUDED

#include "mysqlrouter/base_protocol.h"
#include "mysqlrouter/mysql_session.h"

#include <chrono>
#include <map>
#include <string>

namespace routing {

/** Timeout for idling clients (in seconds).
 *
 * Constant defining how long (in seconds) a client can keep the connection
 * idling. This is similar to the wait_timeout variable in the MySQL Server.
 *
 * 0 == no timeout used.
 */
constexpr int kDefaultWaitTimeout{0};

/** Max number of active routes for this routing instance.
 *
 * 0 == no limit per route
 */
constexpr int kDefaultMaxConnections{0};

/** Timeout connecting to destination (in seconds).
 *
 * Constant defining how long we wait to establish connection with the server
 * before we give up.
 */
constexpr std::chrono::seconds kDefaultDestinationConnectionTimeout{
    mysqlrouter::MySQLSession::kDefaultConnectTimeout};

/** Maximum connect or handshake errors per host.
 *
 * Maximum connect or handshake errors after which a host will be
 * blocked. Such errors can happen when the client does not reply
 * the handshake, sends an incorrect packet, or garbage.
 */
constexpr unsigned long long kDefaultMaxConnectErrors{100};

/**
 * Default bind address used when no bind address is configured.
 */
constexpr std::string_view kDefaultBindAddress{"127.0.0.1"};

/**
 * Default bind address written to the config file during bootstrap.
 */
constexpr std::string_view kDefaultBindAddressBootstrap{"0.0.0.0"};

/** Default net buffer length.
 *
 * Default network buffer length which can be set in the MySQL Server.
 *
 * This should match the default of the latest MySQL Server.
 */
constexpr unsigned int kDefaultNetBufferLength{16384};

/**
 * Timeout waiting for handshake response from client.
 *
 * The number of seconds that MySQL Router waits for a handshake response.
 * The default value is 9 seconds (default MySQL Server minus 1).
 */
constexpr std::chrono::seconds kDefaultClientConnectTimeout{9};

/**
 * delay in milliseconds before an idling connection may be moved to the pool
 * when connection sharing is allowed.
 */
constexpr std::chrono::milliseconds kDefaultConnectionSharingDelay{1000};

/**
 * The number of seconds that MySQL Router waits between checking for
 * reachability of an unreachable destination.
 */
constexpr std::chrono::seconds kDefaultUnreachableDestinationRefreshInterval{1};

/**
 * Default SSL session cache mode.
 */
constexpr bool kDefaultSslSessionCacheMode{true};

/**
 * Default SSL session cache size.
 */
constexpr unsigned int kDefaultSslSessionCacheSize{1024};

/**
 * Default SSL session cache timeout.
 */
constexpr std::chrono::seconds kDefaultSslSessionCacheTimeout{300};

/**
 * Default Connect Retry timeout.
 */
constexpr std::chrono::seconds kDefaultConnectRetryTimeout{7};

/**
 * Default Wait For My Writes timeout.
 */
constexpr bool kDefaultWaitForMyWrites{true};

/**
 * Default Wait For My Writes timeout.
 */
constexpr std::chrono::seconds kDefaultWaitForMyWritesTimeout{2};

/**
 * Default client SSL mode used when none is configured.
 */
constexpr std::string_view kDefaultClientSslMode{""};

/**
 * Default client SSL mode written to the configuration file on bootstrap.
 */
constexpr std::string_view kDefaultClientSslModeBootstrap{"PREFERRED"};

/**
 * Default client SSL cipher written to the configuration file on bootstrap.
 */
constexpr std::string_view kDefaultClientSslCipherBootstrap{""};

/**
 * Default client SSL curves written to the configuration file on bootstrap.
 */
constexpr std::string_view kDefaultClientSslCurvesBootstrap{""};

/**
 * Default client SSL DH params written to the configuration file on bootstrap.
 */
constexpr std::string_view kDefaultClientSslDhParamsBootstrap{""};

/**
 * Default server SSL mode used when none is configured.
 */
constexpr std::string_view kDefaultServerSslMode{"AS_CLIENT"};

/**
 * Default client SSL mode written to the configuration file on bootstrap.
 */
constexpr std::string_view kDefaultServerSslModeBootstrap{"PREFERRED"};

/**
 * Default server SSL verify.
 */
constexpr std::string_view kDefaultServerSslVerify{"DISABLED"};

/**
 * Default server SSL cipher written to the configuration file on bootstrap.
 */
constexpr std::string_view kDefaultServerSslCipherBootstrap{""};

/**
 * Default server SSL curves written to the configuration file on bootstrap.
 */
constexpr std::string_view kDefaultServerSslCurvesBootstrap{""};

/**
 * Default server SSL CA written to the configuration file on bootstrap.
 */
constexpr std::string_view kDefaultServerSslCaBootstrap{""};

/**
 * Default server SSL CA path written to the configuration file on bootstrap.
 */
constexpr std::string_view kDefaultServerSslCaPathBootstrap{""};

/**
 * Default server SSL CRL file written to the configuration file on bootstrap.
 */
constexpr std::string_view kDefaultServerSslCrlFileBootstrap{""};

/**
 * Default server SSL CRL path written to the configuration file on bootstrap.
 */
constexpr std::string_view kDefaultServerSslCrlPathBootstrap{""};

/**
 * Default connection sharing status.
 */
constexpr bool kDefaultConnectionSharing{false};

/**
 * Default maximum total connections handled by all the routing endpoints.
 */
constexpr uint64_t kDefaultMaxTotalConnections{512};

/**
 * Default for the configuration option determining if the Router enforces the
 * router_require attribute of the user.
 */
constexpr bool kDefaultRequireEnforce{true};

enum class RoutingBootstrapSectionType {
  kClassicRw,
  kClassicRo,
  kXRw,
  kXRo,
  kRwSplit
};

constexpr uint16_t kDefaultPortClassicRw{6446};
constexpr uint16_t kDefaultPortClassicRo{6447};
constexpr uint16_t kDefaultPortXRw{6448};
constexpr uint16_t kDefaultPortXRo{6449};
constexpr uint16_t kDefaultPortRwSplit{6450};
// by default sockets are not available
constexpr std::string_view kDefaultNamedSocket{""};

constexpr std::string_view kDefaultClassicRwSectionName{"bootstrap_rw"};
constexpr std::string_view kDefaultClassicRoSectionName{"bootstrap_ro"};
constexpr std::string_view kDefaultXRwSectionName{"bootstrap_x_rw"};
constexpr std::string_view kDefaultXRoSectionName{"bootstrap_x_ro"};
constexpr std::string_view kDefaultRwSplitSectionName{"bootstrap_rw_split"};

/** @brief Modes supported by Routing plugin */
enum class RoutingMode {
  kUndefined = 0,
  kReadWrite = 1,
  kReadOnly = 2,
};

// the declaration of RoutingMode and then renaming to Mode works around a bug
// in doxygen which otherwise reports:
//
// storage/innobase/include/buf0dblwr.h:365: warning:
// documented symbol 'bool dblwr::Mode::is_atomic' was not declared or defined.
using Mode = RoutingMode;

enum class AccessMode {
  kUndefined = 0,
  kAuto = 1,
};

/** @brief Routing strategies supported by Routing plugin */
enum class RoutingStrategy {
  kUndefined = 0,
  kFirstAvailable = 1,
  kNextAvailable = 2,
  kRoundRobin = 3,
  kRoundRobinWithFallback = 4,
};

/**
 * Get comma separated list of all access mode names.
 */
std::string get_access_mode_names();

/**
 * Returns AccessMode for its literal representation.
 *
 * If no AccessMode is found for given string,
 * Mode::kUndefined is returned.
 *
 * @param value literal representation of the access mode
 * @return AccessMode for the given string or AccessMode::kUndefined
 */
AccessMode get_access_mode(const std::string &value);

/**
 * Returns literal name of given access mode.
 *
 * Returns literal name of given access mode as a std:string. When
 * the access mode is not found, empty string is returned.
 *
 * @param access_mode  access_mode to look up
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

RoutingBootstrapSectionType get_section_type_from_routing_name(
    const std::string &name);

BaseProtocol::Type get_default_protocol(
    RoutingBootstrapSectionType section_type);

uint16_t get_default_port(RoutingBootstrapSectionType section_type);

RoutingStrategy get_default_routing_strategy(
    RoutingBootstrapSectionType section_type);

std::string get_destinations_role(RoutingBootstrapSectionType section_type);

std::string get_default_routing_name(RoutingBootstrapSectionType section_type);

AccessMode get_default_access_mode(RoutingBootstrapSectionType section_type);

bool get_default_connection_sharing(RoutingBootstrapSectionType section_type);

bool get_default_router_require_enforce(
    RoutingBootstrapSectionType section_type);

}  // namespace routing

#endif  // MYSQLROUTER_ROUTING_INCLUDED
