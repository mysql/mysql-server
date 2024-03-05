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

#include "mysqlrouter/routing.h"

#include <array>
#include <chrono>
#include <string>

#ifndef _WIN32
#include <netdb.h>        // addrinfo
#include <netinet/tcp.h>  // TCP_NODELAY
#include <sys/socket.h>   // SOCK_NONBLOCK, ...
#else
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

#include "common.h"  // serial_comma
#include "mysql/harness/logging/logging.h"
#include "mysql/harness/net_ts/impl/resolver.h"
#include "mysql/harness/net_ts/impl/socket.h"
#include "mysql/harness/net_ts/impl/socket_error.h"

IMPORT_LOG_FUNCTIONS()

namespace routing {

// unused constant
// const int kMaxConnectTimeout = INT_MAX / 1000;

static const std::array<const char *, 2> kAccessModeNames{{
    nullptr,
    "auto",
}};

AccessMode get_access_mode(const std::string &value) {
  for (unsigned int i = 1; i < kAccessModeNames.size(); ++i)
    if (kAccessModeNames[i] == value) return static_cast<AccessMode>(i);
  return AccessMode::kUndefined;
}

std::string get_access_mode_names() {
  // +1 to skip undefined
  return mysql_harness::serial_comma(kAccessModeNames.begin() + 1,
                                     kAccessModeNames.end());
}

std::string get_access_mode_name(AccessMode mode) noexcept {
  if (mode == AccessMode::kUndefined) return "";

  return kAccessModeNames[static_cast<int>(mode)];
}

// keep in-sync with enum RoutingStrategy
static const std::array<const char *, 5> kRoutingStrategyNames{{
    nullptr,
    "first-available",
    "next-available",
    "round-robin",
    "round-robin-with-fallback",
}};

RoutingStrategy get_routing_strategy(const std::string &value) {
  for (unsigned int i = 1; i < kRoutingStrategyNames.size(); ++i)
    if (kRoutingStrategyNames[i] == value)
      return static_cast<RoutingStrategy>(i);
  return RoutingStrategy::kUndefined;
}

std::string get_routing_strategy_names(bool metadata_cache) {
  // round-robin-with-fallback is not supported for static routing
  const std::array<const char *, 3> kRoutingStrategyNamesStatic{{
      "first-available",
      "next-available",
      "round-robin",
  }};

  // next-available is not supported for metadata-cache routing
  const std::array<const char *, 3> kRoutingStrategyNamesMetadataCache{{
      "first-available",
      "round-robin",
      "round-robin-with-fallback",
  }};

  const auto &v = metadata_cache ? kRoutingStrategyNamesMetadataCache
                                 : kRoutingStrategyNamesStatic;
  return mysql_harness::serial_comma(v.begin(), v.end());
}

std::string get_routing_strategy_name(
    RoutingStrategy routing_strategy) noexcept {
  if (routing_strategy == RoutingStrategy::kUndefined)
    return "<not set>";
  else
    return kRoutingStrategyNames[static_cast<int>(routing_strategy)];
}

RoutingBootstrapSectionType get_section_type_from_routing_name(
    const std::string &name) {
  if (name == kDefaultClassicRwSectionName) {
    return RoutingBootstrapSectionType::kClassicRw;
  } else if (name == kDefaultClassicRoSectionName) {
    return RoutingBootstrapSectionType::kClassicRo;
  } else if (name == kDefaultXRwSectionName) {
    return RoutingBootstrapSectionType::kXRw;
  } else if (name == kDefaultXRoSectionName) {
    return RoutingBootstrapSectionType::kXRo;
  } else if (name == kDefaultXRoSectionName) {
  } else {
  }

  return RoutingBootstrapSectionType::kRwSplit;
}

BaseProtocol::Type get_default_protocol(
    RoutingBootstrapSectionType section_type) {
  if (section_type == RoutingBootstrapSectionType::kXRw ||
      section_type == RoutingBootstrapSectionType::kXRo) {
    return BaseProtocol::Type::kXProtocol;
  }

  return BaseProtocol::Type::kClassicProtocol;
}

uint16_t get_default_port(RoutingBootstrapSectionType section_type) {
  switch (section_type) {
    case RoutingBootstrapSectionType::kClassicRw:
      return kDefaultPortClassicRw;
    case RoutingBootstrapSectionType::kClassicRo:
      return kDefaultPortClassicRo;
    case RoutingBootstrapSectionType::kXRw:
      return kDefaultPortXRw;
    case RoutingBootstrapSectionType::kXRo:
      return kDefaultPortXRo;
    default:
      assert(section_type == RoutingBootstrapSectionType::kRwSplit);
  }

  return routing::kDefaultPortRwSplit;
}

RoutingStrategy get_default_routing_strategy(
    RoutingBootstrapSectionType section_type) {
  switch (section_type) {
    case RoutingBootstrapSectionType::kClassicRw:
    case RoutingBootstrapSectionType::kXRw:
      return RoutingStrategy::kFirstAvailable;
    case RoutingBootstrapSectionType::kClassicRo:
    case RoutingBootstrapSectionType::kXRo:
      return RoutingStrategy::kRoundRobinWithFallback;
    default:
      assert(section_type == RoutingBootstrapSectionType::kRwSplit);
  }

  return RoutingStrategy::kRoundRobin;
}

std::string get_destinations_role(
    routing::RoutingBootstrapSectionType section_type) {
  switch (section_type) {
    case RoutingBootstrapSectionType::kClassicRw:
    case RoutingBootstrapSectionType::kXRw:
      return "PRIMARY";
    case RoutingBootstrapSectionType::kClassicRo:
    case RoutingBootstrapSectionType::kXRo:
      return "SECONDARY";
    default:
      assert(section_type == RoutingBootstrapSectionType::kRwSplit);
  }

  return "PRIMARY_AND_SECONDARY";
}

std::string get_default_routing_name(RoutingBootstrapSectionType section_type) {
  switch (section_type) {
    case RoutingBootstrapSectionType::kClassicRw:
      return std::string(kDefaultClassicRwSectionName);
    case RoutingBootstrapSectionType::kClassicRo:
      return std::string(kDefaultClassicRoSectionName);
    case RoutingBootstrapSectionType::kXRw:
      return std::string(kDefaultXRwSectionName);
    case RoutingBootstrapSectionType::kXRo:
      return std::string(kDefaultXRoSectionName);
    default:
      assert(section_type == RoutingBootstrapSectionType::kRwSplit);
  }

  return std::string(kDefaultRwSplitSectionName);
}

AccessMode get_default_access_mode(RoutingBootstrapSectionType section_type) {
  if (section_type == RoutingBootstrapSectionType::kRwSplit) {
    return AccessMode::kAuto;
  }

  return AccessMode::kUndefined;
}

bool get_default_connection_sharing(RoutingBootstrapSectionType section_type) {
  return section_type == RoutingBootstrapSectionType::kRwSplit;
}

bool get_default_router_require_enforce(
    RoutingBootstrapSectionType section_type) {
  // default is true for classic, false for x protocol
  return (section_type != RoutingBootstrapSectionType::kXRw) &&
         (section_type != RoutingBootstrapSectionType::kXRo);
}

}  // namespace routing
