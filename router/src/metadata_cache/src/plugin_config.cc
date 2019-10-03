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
      {"router_id", "0"},
      {"thread_stack_size",
       to_string(mysql_harness::kDefaultStackSizeInKiloBytes)},
      {"use_gr_notifications", "0"},
      {"cluster_type", "gr"}};
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

std::string MetadataCachePluginConfig::get_cluster_type_specific_id() const {
  if (metadata_cache_dynamic_state) {
    metadata_cache_dynamic_state->load();
    return metadata_cache_dynamic_state->get_cluster_type_specific_id();
  }

  return "";
}

unsigned MetadataCachePluginConfig::get_view_id() const {
  if (metadata_cache_dynamic_state) {
    metadata_cache_dynamic_state->load();
    return metadata_cache_dynamic_state->get_view_id();
  }

  return 0;
}

std::vector<mysql_harness::TCPAddress>
MetadataCachePluginConfig::get_metadata_servers(
    const mysql_harness::ConfigSection *section, uint16_t default_port) const {
  std::vector<mysql_harness::TCPAddress> address_vector;

  auto add_metadata_server = [&](const std::string &address) {
    mysqlrouter::URI u(address);
    if (u.port == 0) u.port = default_port;

    // push_back calls TCPAddress ctor, which queries DNS in order to determine
    // IP address familiy (IPv4 or IPv6)
    log_debug("Adding metadata server '%s:%u', also querying DNS ...",
              u.host.c_str(), u.port);
    address_vector.push_back(mysql_harness::TCPAddress(u.host, u.port));
    log_debug("Done adding metadata server '%s:%u'", u.host.c_str(), u.port);
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

mysqlrouter::ClusterType MetadataCachePluginConfig::get_cluster_type(
    const mysql_harness::ConfigSection *section) {
  std::string value = get_option_string(section, "cluster_type");
  if (value == "ar") {
    return mysqlrouter::ClusterType::AR_V2;
  } else if (value == "gr") {
    return mysqlrouter::ClusterType::GR_V2;
  }

  throw invalid_argument(get_log_prefix("cluster_type") + " is incorrect '" +
                         value + "', expected 'ar' or 'gr'");
}

std::unique_ptr<ClusterMetadataDynamicState>
MetadataCachePluginConfig::get_dynamic_state(
    const mysql_harness::ConfigSection *section) {
  bool use_dynamic_state = mysql_harness::DIM::instance().is_DynamicState();
  if (!use_dynamic_state) {
    return nullptr;
  }

  auto &dynamic_state_base = mysql_harness::DIM::instance().get_DynamicState();
  return std::make_unique<ClusterMetadataDynamicState>(
      &dynamic_state_base, get_cluster_type(section));
}
