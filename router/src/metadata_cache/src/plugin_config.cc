/*
  Copyright (c) 2016, 2024, Oracle and/or its affiliates.

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

#include "plugin_config.h"

#include <algorithm>
#include <array>
#include <climits>
#include <exception>
#include <map>
#include <stdexcept>
#include <variant>
#include <vector>

#include "dim.h"
#include "mysql/harness/dynamic_config.h"
#include "mysql/harness/logging/logging.h"
#include "mysql/harness/utility/string.h"  // string_format
#include "mysqlrouter/metadata_cache.h"
#include "mysqlrouter/supported_metadata_cache_options.h"
#include "mysqlrouter/uri.h"
#include "mysqlrouter/utils.h"  // ms_to_second_string
IMPORT_LOG_FUNCTIONS()

using mysql_harness::utility::string_format;
using mysqlrouter::ms_to_seconds_string;
using mysqlrouter::to_string;

using MilliSecondsOption = mysql_harness::MilliSecondsOption;
using StringOption = mysql_harness::StringOption;

template <typename T>
using IntOption = mysql_harness::IntOption<T>;

std::string MetadataCachePluginConfig::get_default(
    std::string_view option) const {
  static const std::map<std::string_view, std::string> defaults{
      {"address", std::string{metadata_cache::kDefaultMetadataAddress}},
      {"ttl", ms_to_seconds_string(mysqlrouter::kDefaultMetadataTTLCluster)},
      {"auth_cache_ttl",
       ms_to_seconds_string(metadata_cache::kDefaultAuthCacheTTL)},
      {"auth_cache_refresh_interval",
       ms_to_seconds_string(metadata_cache::kDefaultAuthCacheRefreshInterval)},
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

bool MetadataCachePluginConfig::is_required(std::string_view option) const {
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

std::string MetadataCachePluginConfig::get_clusterset_id() const {
  if (metadata_cache_dynamic_state) {
    metadata_cache_dynamic_state->load();
    return metadata_cache_dynamic_state->get_clusterset_id();
  }

  return "";
}

uint64_t MetadataCachePluginConfig::get_view_id() const {
  if (metadata_cache_dynamic_state) {
    metadata_cache_dynamic_state->load();
    return metadata_cache_dynamic_state->get_view_id();
  }

  return 0;
}

std::vector<mysql_harness::TCPAddress>
MetadataCachePluginConfig::get_metadata_servers(uint16_t default_port) const {
  std::vector<mysql_harness::TCPAddress> address_vector;

  auto add_metadata_server = [&](const std::string &address) {
    mysqlrouter::URI u(address);
    if (u.port == 0) u.port = default_port;

    // emplace_back calls TCPAddress ctor, which queries DNS in order to
    // determine IP address family (IPv4 or IPv6)
    log_debug("Adding metadata server '%s:%u', also querying DNS ...",
              u.host.c_str(), u.port);
    address_vector.emplace_back(u.host, u.port);
    log_debug("Done adding metadata server '%s:%u'", u.host.c_str(), u.port);
  };

  if (metadata_cache_dynamic_state) {
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
        throw std::invalid_argument(
            std::string("cluster-metadata-servers is incorrect (") +
            exc.what() + ")");
      }
    }
  }

  return address_vector;
}

class ClusterTypeOption {
 public:
  mysqlrouter::ClusterType operator()(const std::string &value,
                                      const std::string &option_desc) {
    if (value == "rs") {
      return mysqlrouter::ClusterType::RS_V2;
    } else if (value == "gr") {
      return mysqlrouter::ClusterType::GR_V2;
    }

    throw std::invalid_argument(option_desc + " is incorrect '" + value +
                                "', expected 'rs' or 'gr'");
  }
};

#define GET_OPTION_CHECKED(option, section, name, value) \
  static_assert(mysql_harness::str_in_collection(        \
      metadata_cache_supported_options, name));          \
  option = get_option(section, name, value);

std::unique_ptr<ClusterMetadataDynamicState>
MetadataCachePluginConfig::get_dynamic_state(
    const mysql_harness::ConfigSection *section) {
  bool use_dynamic_state = mysql_harness::DIM::instance().is_DynamicState();
  if (!use_dynamic_state) {
    return nullptr;
  }

  auto &dynamic_state_base = mysql_harness::DIM::instance().get_DynamicState();
  mysqlrouter::ClusterType cluster_type;
  GET_OPTION_CHECKED(cluster_type, section, "cluster_type",
                     ClusterTypeOption{});
  return std::make_unique<ClusterMetadataDynamicState>(&dynamic_state_base,
                                                       cluster_type);
}

MetadataCachePluginConfig::MetadataCachePluginConfig(
    const mysql_harness::ConfigSection *section)
    : BasePluginConfig(section),
      metadata_cache_dynamic_state(get_dynamic_state(section)),
      metadata_servers_addresses(
          get_metadata_servers(metadata_cache::kDefaultMetadataPort)) {
  GET_OPTION_CHECKED(user, section, "user", StringOption{});
  auto ttl_op = MilliSecondsOption{0.0, 3600.0};
  GET_OPTION_CHECKED(ttl, section, "ttl", ttl_op);
  auto auth_cache_ttl_op = MilliSecondsOption{-1, 3600.0};
  GET_OPTION_CHECKED(auth_cache_ttl, section, "auth_cache_ttl",
                     auth_cache_ttl_op);
  auto auth_cache_refresh_interval_op = MilliSecondsOption{0.001, 3600.0};
  GET_OPTION_CHECKED(auth_cache_refresh_interval, section,
                     "auth_cache_refresh_interval",
                     auth_cache_refresh_interval_op);
  GET_OPTION_CHECKED(cluster_name, section, "metadata_cluster", StringOption{});
  GET_OPTION_CHECKED(connect_timeout, section, "connect_timeout",
                     IntOption<uint16_t>{1});
  GET_OPTION_CHECKED(read_timeout, section, "read_timeout",
                     IntOption<uint16_t>{1});
  auto thread_stack_size_op = IntOption<uint32_t>{1, 65535};
  GET_OPTION_CHECKED(thread_stack_size, section, "thread_stack_size",
                     thread_stack_size_op);
  GET_OPTION_CHECKED(use_gr_notifications, section, "use_gr_notifications",
                     IntOption<bool>{});
  GET_OPTION_CHECKED(cluster_type, section, "cluster_type",
                     ClusterTypeOption{});
  GET_OPTION_CHECKED(router_id, section, "router_id", IntOption<uint32_t>{});

  if (cluster_type == mysqlrouter::ClusterType::RS_V2 &&
      section->has("use_gr_notifications")) {
    throw std::invalid_argument(
        "option 'use_gr_notifications' is not valid for cluster type 'rs'");
  }
  if (auth_cache_ttl > std::chrono::seconds(-1) &&
      auth_cache_ttl < std::chrono::milliseconds(1)) {
    throw std::invalid_argument(
        "'auth_cache_ttl' option value '" +
        get_option(section, "auth_cache_ttl", StringOption{}) +
        "' should be in range 0.001 and 3600 inclusive or -1 for "
        "auth_cache_ttl disabled");
  }
}

static double duration_to_double(const auto &duration) {
  return std::chrono::duration_cast<std::chrono::duration<double>>(duration)
      .count();
}

void MetadataCachePluginConfig::expose_initial_configuration() const {
  using DC = mysql_harness::DynamicConfig;
  // the metadata_cache options are split into 2 groups:
  // 1. metadata_cache - router config related
  // 2. routing - cluster/replicaset routing related
  const DC::SectionId metadata_cache_id{"metadata_cache", ""};
  const DC::SectionId routing_id{"routing_rules", ""};

  auto set_mdc_option = [&](const std::string &option, const auto &value) {
    DC::instance().set_option_configured(metadata_cache_id, option, value);
  };

  auto set_routing_option = [&](const std::string &option, const auto &value) {
    DC::instance().set_option_configured(routing_id, option, value);
  };

  set_mdc_option("user", user);
  set_mdc_option("ttl", duration_to_double(ttl));
  set_mdc_option("auth_cache_ttl", duration_to_double(auth_cache_ttl));
  set_mdc_option("auth_cache_refresh_interval",
                 duration_to_double(auth_cache_refresh_interval));
  // set_mdc_option("cluster_name", cluster_name);
  set_mdc_option("connect_timeout", connect_timeout);
  set_mdc_option("read_timeout", read_timeout);
  set_mdc_option("use_gr_notifications", use_gr_notifications);
  // set_mdc_option("cluster_type", mysqlrouter::to_string(cluster_type));

  // This is the id in the table storing that data so we don't want redundancy
  // there
  // set_mdc_option("router_id", router_id);

  if (!target_cluster.empty()) {
    set_routing_option("target_cluster", target_cluster);
  }

  set_routing_option("invalidated_cluster_policy",
                     mysqlrouter::to_string(invalidated_cluster_policy));
  set_routing_option("use_replica_primary_as_rw", use_replica_primary_as_rw);
  set_routing_option("unreachable_quorum_allowed_traffic",
                     to_string(unreachable_quorum_allowed_traffic));
  set_routing_option("stats_updates_frequency",
                     stats_updates_frequency.count());
  set_routing_option("read_only_targets", to_string(read_only_targets));
}

void MetadataCachePluginConfig::expose_default_configuration() const {
  using DC = mysql_harness::DynamicConfig;
  const DC::SectionId metadata_cache_id{"metadata_cache", ""};
  const DC::SectionId routing_id{"routing_rules", ""};

  auto set_mdc_default = [&](const std::string &option,
                             const auto &default_value) {
    DC::instance().set_option_default(metadata_cache_id, option, default_value);
  };

  auto set_mdc_default2 = [&](const std::string &option,
                              const auto &default_value_cluster,
                              const auto &default_value_clusterset) {
    DC::instance().set_option_default(metadata_cache_id, option,
                                      default_value_cluster,
                                      default_value_clusterset);
  };

  auto set_routing_default = [&](const std::string &option,
                                 const auto &default_value) {
    DC::instance().set_option_default(routing_id, option, default_value);
  };

  auto set_routing_default2 = [&](const std::string &option,
                                  const auto &default_value_cluster,
                                  const auto &default_value_clusterset) {
    DC::instance().set_option_default(routing_id, option, default_value_cluster,
                                      default_value_clusterset);
  };

  // set_mdc_default("user", user, std::monostate{});
  set_mdc_default2(
      "ttl", duration_to_double(mysqlrouter::kDefaultMetadataTTLCluster),
      duration_to_double(mysqlrouter::kDefaultMetadataTTLClusterSet));
  set_mdc_default("auth_cache_ttl",
                  duration_to_double(metadata_cache::kDefaultAuthCacheTTL));

  // for clusterset default is smaller than default TTL so it is getting bumped
  // to default TTL
  static_assert(mysqlrouter::kDefaultMetadataTTLClusterSet >=
                metadata_cache::kDefaultAuthCacheRefreshInterval);
  set_mdc_default2(
      "auth_cache_refresh_interval",
      duration_to_double(metadata_cache::kDefaultAuthCacheRefreshInterval),
      duration_to_double(mysqlrouter::kDefaultMetadataTTLClusterSet));
  // we do not share cluster_name
  // set_mdc_default("cluster_name", cluster_name, std::monostate{});
  set_mdc_default("connect_timeout", metadata_cache::kDefaultConnectTimeout);
  set_mdc_default("read_timeout", metadata_cache::kDefaultReadTimeout);
  set_mdc_default2("use_gr_notifications",
                   mysqlrouter::kDefaultUseGRNotificationsCluster,
                   mysqlrouter::kDefaultUseGRNotificationsClusterSet);
  // we don't share cluster_type nor router_id
  // set_mdc_default("cluster_type", mysqlrouter::to_string(cluster_type));
  // set_mdc_default("router_id", router_id);

  set_routing_default2("target_cluster", std::monostate{}, target_cluster);
  set_routing_default(
      "invalidated_cluster_policy",
      mysqlrouter::to_string(kDefautlInvalidatedClusterRoutingPolicy));
  set_routing_default("use_replica_primary_as_rw", false);
  set_routing_default("unreachable_quorum_allowed_traffic",
                      to_string(kDefaultQuorumConnectionLostAllowTraffic));
  set_routing_default("stats_updates_frequency", -1);
  set_routing_default("read_only_targets", to_string(kDefaultReadOnlyTargets));
}
