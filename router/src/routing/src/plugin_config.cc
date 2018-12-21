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

#include "plugin_config.h"
#include "mysql/harness/string_utils.h"
#include "mysql_routing.h"
#include "mysqlrouter/metadata_cache.h"
#include "mysqlrouter/routing.h"

#include <algorithm>
#include <exception>
#include <stdexcept>
#include <vector>

#include "mysqlrouter/utils.h"

using mysqlrouter::URI;
using mysqlrouter::URIError;
using mysqlrouter::to_string;
using std::invalid_argument;
using std::string;
using std::vector;

/** @brief Constructor
 *
 * @param section from configuration file provided as ConfigSection
 */
RoutingPluginConfig::RoutingPluginConfig(
    const mysql_harness::ConfigSection *section)
    : BasePluginConfig(section),
      metadata_cache_(false),
      protocol(get_protocol(section, "protocol")),
      destinations(get_option_destinations(section, "destinations", protocol)),
      bind_port(get_option_tcp_port(section, "bind_port")),
      bind_address(
          get_option_tcp_address(section, "bind_address", false, bind_port)),
      named_socket(get_option_named_socket(section, "socket")),
      connect_timeout(get_uint_option<uint16_t>(section, "connect_timeout", 1)),
      mode(get_option_mode(section, "mode")),
      routing_strategy(
          get_option_routing_strategy(section, "routing_strategy")),
      max_connections(get_uint_option<uint16_t>(section, "max_connections", 1)),
      max_connect_errors(get_uint_option<uint32_t>(
          section, "max_connect_errors", 1, UINT32_MAX)),
      client_connect_timeout(get_uint_option<uint32_t>(
          section, "client_connect_timeout", 2, 31536000)),
      net_buffer_length(get_uint_option<uint32_t>(section, "net_buffer_length",
                                                  1024, 1048576)),
      thread_stack_size(
          get_uint_option<uint32_t>(section, "thread_stack_size", 1, 65535)) {
  // either bind_address or socket needs to be set, or both
  if (!bind_address.port && !named_socket.is_set()) {
    throw invalid_argument(
        "either bind_address or socket option needs to be supplied, or both");
  }
}

string RoutingPluginConfig::get_default(const string &option) const {
  const std::map<string, string> defaults{
      {"bind_address", to_string(routing::kDefaultBindAddress)},
      {"connect_timeout",
       to_string(std::chrono::duration_cast<std::chrono::seconds>(
                     routing::kDefaultDestinationConnectionTimeout)
                     .count())},
      {"max_connections", to_string(routing::kDefaultMaxConnections)},
      {"max_connect_errors", to_string(routing::kDefaultMaxConnectErrors)},
      {"client_connect_timeout",
       to_string(std::chrono::duration_cast<std::chrono::seconds>(
                     routing::kDefaultClientConnectTimeout)
                     .count())},
      {"net_buffer_length", to_string(routing::kDefaultNetBufferLength)},
      {"thread_stack_size",
       to_string(mysql_harness::kDefaultStackSizeInKiloBytes)},
  };

  auto it = defaults.find(option);
  if (it == defaults.end()) {
    return string();
  }
  return it->second;
}

bool RoutingPluginConfig::is_required(const string &option) const {
  const vector<string> required{
      "destinations",
      // at least one of those is required, we handle this logic in get_option_
      // methods:
      "routing_strategy",
      "mode",
  };

  return std::find(required.begin(), required.end(), option) != required.end();
}

routing::AccessMode RoutingPluginConfig::get_option_mode(
    const mysql_harness::ConfigSection *section, const string &option) const {
  string value;
  try {
    value = get_option_string(section, option);
  } catch (const mysqlrouter::option_not_present &) {
    // no mode given, that's fine, it is no longer required
    return routing::AccessMode::kUndefined;
  }

  std::transform(value.begin(), value.end(), value.begin(), ::tolower);

  // if the mode is given it still needs to be valid
  routing::AccessMode result = routing::get_access_mode(value);
  if (result == routing::AccessMode::kUndefined) {
    const string valid = routing::get_access_mode_names();
    throw invalid_argument(get_log_prefix(option) + " is invalid; valid are " +
                           valid + " (was '" + value + "')");
  }
  return result;
}

routing::RoutingStrategy RoutingPluginConfig::get_option_routing_strategy(
    const mysql_harness::ConfigSection *section, const string &option) const {
  string value;
  try {
    value = get_option_string(section, option);
  } catch (const mysqlrouter::option_not_present &) {
    // routing_strategy option is not given
    // this is fine as long as mode is set which means that we deal with an old
    // configuration which we still want to support

    if (mode == routing::AccessMode::kUndefined) {
      throw;
    }

    /** @brief `mode` option read from configuration section */
    return routing::RoutingStrategy::kUndefined;
  }

  std::transform(value.begin(), value.end(), value.begin(), ::tolower);

  auto result = routing::get_routing_strategy(value);
  if (result == routing::RoutingStrategy::kUndefined ||
      ((result == routing::RoutingStrategy::kRoundRobinWithFallback) &&
       !metadata_cache_)) {
    const string valid = routing::get_routing_strategy_names(metadata_cache_);
    throw invalid_argument(get_log_prefix(option) + " is invalid; valid are " +
                           valid + " (was '" + value + "')");
  }
  return result;
}

Protocol::Type RoutingPluginConfig::get_protocol(
    const mysql_harness::ConfigSection *section,
    const std::string &option) const {
  std::string name;
  try {
    name = section->get(option);
  } catch (const mysql_harness::bad_option &) {
    return Protocol::get_default();
  }

  std::transform(name.begin(), name.end(), name.begin(), ::tolower);

  return Protocol::get_by_name(name);
}

string RoutingPluginConfig::get_option_destinations(
    const mysql_harness::ConfigSection *section, const string &option,
    const Protocol::Type &protocol_type) const {
  bool required = is_required(option);
  string value;

  try {
    value = section->get(option);
  } catch (const mysql_harness::bad_option &) {
    if (required) {
      throw invalid_argument(get_log_prefix(option) + " is required");
    }
  }

  if (value.empty()) {
    if (required) {
      throw invalid_argument(get_log_prefix(option) +
                             " is required and needs a value");
    }
    value = get_default(option);
  }

  try {
    // disable root-less paths like mailto:foo@example.org to stay
    // backward compatible with
    //
    //   localhost:1234,localhost:1235
    //
    // which parse into:
    //
    //   scheme: localhost
    //   path: 1234,localhost:1235
    auto uri = URI(value,  // raises URIError when URI is invalid
                   false   // allow_path_rootless
    );
    if (uri.scheme == "metadata-cache") {
      metadata_cache_ = true;
    } else {
      throw invalid_argument(get_log_prefix(option) +
                             " has an invalid URI scheme '" + uri.scheme +
                             "' for URI " + value);
    }
    return value;
  } catch (URIError &) {
    char delimiter = ',';

    mysql_harness::trim(value);
    if (value.back() == delimiter || value.front() == delimiter) {
      throw invalid_argument(
          get_log_prefix(option) +
          ": empty address found in destination list (was '" + value + "')");
    }

    std::stringstream ss(value);
    std::string part;
    std::pair<std::string, uint16_t> info;
    while (std::getline(ss, part, delimiter)) {
      mysql_harness::trim(part);
      if (part.empty()) {
        throw invalid_argument(
            get_log_prefix(option) +
            ": empty address found in destination list (was '" + value + "')");
      }
      try {
        info = mysqlrouter::split_addr_port(part);
      } catch (const std::runtime_error &e) {
        throw invalid_argument(get_log_prefix(option) +
                               ": address in destination list '" + part +
                               "' is invalid: " + e.what());
      }
      if (info.second == 0) {
        info.second = Protocol::get_default_port(protocol_type);
      }
      mysql_harness::TCPAddress addr(info.first, info.second);
      if (!addr.is_valid()) {
        throw invalid_argument(get_log_prefix(option) +
                               " has an invalid destination address '" +
                               addr.str() + "'");
      }
    }
  }

  return value;
}
