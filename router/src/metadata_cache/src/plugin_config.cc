/*
  Copyright (c) 2016, 2019, Oracle and/or its affiliates. All rights reserved.

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
#include "mysqlrouter/metadata_cache.h"
#include "mysqlrouter/uri.h"
#include "mysqlrouter/utils.h"

#include <limits.h>
#include <algorithm>
#include <cerrno>
#include <exception>
#include <map>
#include <stdexcept>
#include <vector>

#include "dim.h"
#include "mysql/harness/logging/logging.h"
IMPORT_LOG_FUNCTIONS()

using mysqlrouter::ms_to_seconds_string;
using mysqlrouter::string_format;
using mysqlrouter::to_string;
using std::invalid_argument;

std::string MetadataCachePluginConfig::get_default(
    const std::string &option) const {
  static const std::map<std::string, std::string> defaults{
      {"address", metadata_cache::kDefaultMetadataAddress},
      {"ttl", ms_to_seconds_string(metadata_cache::kDefaultMetadataTTL)},
      {"connect_timeout", to_string(metadata_cache::kDefaultConnectTimeout)},
      {"read_timeout", to_string(metadata_cache::kDefaultReadTimeout)},
      {"thread_stack_size",
       to_string(mysql_harness::kDefaultStackSizeInKiloBytes)}};
  auto it = defaults.find(option);
  if (it == defaults.end()) {
    return std::string();
  }
  return it->second;
}

bool MetadataCachePluginConfig::is_required(const std::string &option) const {
  const std::vector<std::string> required{
      "user",
  };

  return std::find(required.begin(), required.end(), option) != required.end();
}

std::string MetadataCachePluginConfig::get_group_replication_id() const {
  if (metadata_cache_dynamic_state) {
    metadata_cache_dynamic_state->load();
    return metadata_cache_dynamic_state->get_gr_id();
  }

  return "";
}

std::vector<mysql_harness::TCPAddress>
MetadataCachePluginConfig::get_metadata_servers(
    const mysql_harness::ConfigSection *section, uint16_t default_port) const {
  std::vector<mysql_harness::TCPAddress> address_vector;

  auto add_metadata_server = [&](const std::string &address) {
    std::pair<std::string, uint16_t> bind_info;
    mysqlrouter::URI u(address);
    bind_info.first = u.host;
    bind_info.second = u.port;
    if (bind_info.second == 0) {
      bind_info.second = default_port;
    }

    // push_back calls TCPAddress ctor, which queries DNS in order to determine
    // IP address familiy (IPv4 or IPv6)
    log_debug("Adding metadata server '%s:%u', also querying DNS ...",
              bind_info.first.c_str(), bind_info.second);
    address_vector.push_back(
        mysql_harness::TCPAddress(bind_info.first, bind_info.second));
    log_debug("Done adding metadata server '%s:%u'", bind_info.first.c_str(),
              bind_info.second);
  };

  if (metadata_cache_dynamic_state) {
    if (section->has("bootstrap_server_addresses")) {
      throw std::runtime_error(
          "bootstrap_server_addresses is not allowed when dynamic state file "
          "is used");
    }

    metadata_cache_dynamic_state->load();
    // we do the save right away to check whether we have a write permission to
    // the state file and if not to get an early error report and close the
    // Router
    metadata_cache_dynamic_state->save();

    auto metadata_servers =
        metadata_cache_dynamic_state->get_metadata_servers();
    std::pair<std::string, uint16_t> bind_info;

    for (const auto &address : metadata_servers) {
      try {
        add_metadata_server(address);
      } catch (const std::runtime_error &exc) {
        throw invalid_argument(
            std::string("cluster-metadata-servers is incorrect (") +
            exc.what() + ")");
      }
    }
  } else {
    const std::string option = "bootstrap_server_addresses";
    std::string value = get_option_string(section, option);
    std::stringstream ss(value);
    std::string address;

    // Fetch the string that contains the list of bootstrap servers separated
    // by a delimiter (,).
    while (getline(ss, address, ',')) {
      try {
        add_metadata_server(address);
      } catch (const std::runtime_error &exc) {
        throw invalid_argument(get_log_prefix(option) + " is incorrect (" +
                               exc.what() + ")");
      }
    }
  }

  return address_vector;
}

ClusterMetadataDynamicState *MetadataCachePluginConfig::get_dynamic_state() {
  bool use_dynamic_state = mysql_harness::DIM::instance().is_DynamicState();
  if (!use_dynamic_state) {
    return nullptr;
  }

  auto &dynamic_state_base = mysql_harness::DIM::instance().get_DynamicState();
  return new ClusterMetadataDynamicState(&dynamic_state_base);
}
