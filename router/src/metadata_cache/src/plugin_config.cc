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
#include "mysql/harness/section_config_exposer.h"
#include "mysql/harness/utility/string.h"  // string_format
#include "mysqlrouter/metadata_cache.h"
#include "mysqlrouter/supported_metadata_cache_options.h"
#include "mysqlrouter/uri.h"
#include "mysqlrouter/utils.h"  // ms_to_second_string
IMPORT_LOG_FUNCTIONS()

using mysql_harness::utility::string_format;
using mysqlrouter::ms_to_seconds_string;
using mysqlrouter::to_string;

namespace {
constexpr std::string_view kDefaultSslMode{"PREFERRED"};
constexpr std::string_view kDefaultSslCipher{""};
constexpr std::string_view kDefaultTlsVersion{""};
constexpr std::string_view kDefaultSslCa{""};
constexpr std::string_view kDefaultSslCaPath{""};
constexpr std::string_view kDefaultSslCrl{""};
constexpr std::string_view kDefaultSslCrlPath{""};
}  // namespace

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

static std::string get_ssl_option(const mysql_harness::ConfigSection *section,
                                  const std::string &key,
                                  const std::string_view def_value) {
  if (section->has(key)) return section->get(key);
  return std::string(def_value);
}

#define GET_SSL_OPTION_CHECKED(option, section, name, def_value) \
  static_assert(mysql_harness::str_in_collection(                \
      metadata_cache_supported_options, name));                  \
  option = get_ssl_option(section, name, def_value);

static mysqlrouter::SSLOptions make_ssl_options(
    const mysql_harness::ConfigSection *section) {
  mysqlrouter::SSLOptions options;

  GET_SSL_OPTION_CHECKED(options.mode, section, "ssl_mode", kDefaultSslMode);
  GET_SSL_OPTION_CHECKED(options.cipher, section, "ssl_cipher",
                         kDefaultSslCipher);
  GET_SSL_OPTION_CHECKED(options.tls_version, section, "tls_version",
                         kDefaultTlsVersion);
  GET_SSL_OPTION_CHECKED(options.ca, section, "ssl_ca", kDefaultSslCa);
  GET_SSL_OPTION_CHECKED(options.capath, section, "ssl_capath",
                         kDefaultSslCaPath);
  GET_SSL_OPTION_CHECKED(options.crl, section, "ssl_crl", kDefaultSslCrl);
  GET_SSL_OPTION_CHECKED(options.crlpath, section, "ssl_crlpath",
                         kDefaultSslCrlPath);

  return options;
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

  ssl_options = make_ssl_options(section);

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

namespace {

class MetadataCacheConfigExposer : public mysql_harness::SectionConfigExposer {
 public:
  MetadataCacheConfigExposer(
      const bool initial, const MetadataCachePluginConfig &plugin_config,
      const mysql_harness::ConfigSection &default_section)
      : mysql_harness::SectionConfigExposer(initial, default_section,
                                            {"metadata_cache", ""}),
        plugin_config_(plugin_config) {}

  void expose() override {
    expose_option("user", plugin_config_.user, std::monostate{});
    expose_option(
        "ttl", duration_to_double(plugin_config_.ttl),
        duration_to_double(mysqlrouter::kDefaultMetadataTTLCluster),
        duration_to_double(mysqlrouter::kDefaultMetadataTTLClusterSet), false);
    expose_option("auth_cache_ttl",
                  duration_to_double(plugin_config_.auth_cache_ttl),
                  duration_to_double(metadata_cache::kDefaultAuthCacheTTL));

    // for clusterset default is smaller than default TTL so it is getting
    // bumped to default TTL
    static_assert(mysqlrouter::kDefaultMetadataTTLClusterSet >=
                  metadata_cache::kDefaultAuthCacheRefreshInterval);
    expose_option(
        "auth_cache_refresh_interval",
        duration_to_double(plugin_config_.auth_cache_refresh_interval),
        duration_to_double(metadata_cache::kDefaultAuthCacheRefreshInterval),
        duration_to_double(mysqlrouter::kDefaultMetadataTTLClusterSet), false);
    expose_option("connect_timeout", plugin_config_.connect_timeout,
                  metadata_cache::kDefaultConnectTimeout);
    expose_option("read_timeout", plugin_config_.read_timeout,
                  metadata_cache::kDefaultReadTimeout, true);
    expose_option("use_gr_notifications", plugin_config_.use_gr_notifications,
                  mysqlrouter::kDefaultUseGRNotificationsCluster,
                  mysqlrouter::kDefaultUseGRNotificationsClusterSet, false);

    expose_option(
        "thread_stack_size", plugin_config_.thread_stack_size,
        static_cast<int64_t>(mysql_harness::kDefaultStackSizeInKiloBytes));

    expose_option("ssl_mode", plugin_config_.ssl_options.mode,
                  std::string(kDefaultSslMode));
    expose_option("ssl_cipher", plugin_config_.ssl_options.cipher,
                  std::string(kDefaultSslCipher));
    expose_option("tls_version", plugin_config_.ssl_options.tls_version,
                  std::string(kDefaultTlsVersion));
    expose_option("ssl_ca", plugin_config_.ssl_options.ca,
                  std::string(kDefaultSslCa));
    expose_option("ssl_capath", plugin_config_.ssl_options.capath,
                  std::string(kDefaultSslCaPath));
    expose_option("ssl_crl", plugin_config_.ssl_options.crl,
                  std::string(kDefaultSslCrl));
    expose_option("ssl_crlpath", plugin_config_.ssl_options.crlpath,
                  std::string(kDefaultSslCrlPath));
  }

 private:
  const MetadataCachePluginConfig &plugin_config_;
};

class RoutingRulesConfigExposer : public mysql_harness::SectionConfigExposer {
 public:
  RoutingRulesConfigExposer(const bool initial,
                            const MetadataCachePluginConfig &plugin_config,
                            const mysql_harness::ConfigSection &default_section)
      : mysql_harness::SectionConfigExposer(initial, default_section,
                                            {"routing_rules", ""}),
        plugin_config_(plugin_config) {}

  void expose() override {
    expose_option("target_cluster",
                  plugin_config_.target_cluster.empty()
                      ? OptionValue(std::monostate{})
                      : OptionValue(plugin_config_.target_cluster),
                  std::monostate{}, plugin_config_.target_cluster, false);
    expose_option(
        "invalidated_cluster_policy",
        mysqlrouter::to_string(plugin_config_.invalidated_cluster_policy),
        mysqlrouter::to_string(kDefautlInvalidatedClusterRoutingPolicy));

    expose_option("use_replica_primary_as_rw",
                  plugin_config_.use_replica_primary_as_rw, false);
    expose_option("unreachable_quorum_allowed_traffic",
                  to_string(plugin_config_.unreachable_quorum_allowed_traffic),
                  to_string(kDefaultQuorumConnectionLostAllowTraffic));
    expose_option("stats_updates_frequency",
                  plugin_config_.stats_updates_frequency.count(), -1);
    expose_option("read_only_targets",
                  to_string(plugin_config_.read_only_targets),
                  to_string(kDefaultReadOnlyTargets));
  }

 private:
  const MetadataCachePluginConfig &plugin_config_;
};

}  // namespace

void MetadataCachePluginConfig::expose_configuration(
    const mysql_harness::ConfigSection &default_section,
    const bool initial) const {
  // the metadata_cache options are split into 2 groups:
  // 1. metadata_cache - router config related
  MetadataCacheConfigExposer(initial, *this, default_section).expose();

  // 2. routing - cluster/replicaset routing related
  RoutingRulesConfigExposer(initial, *this, default_section).expose();
}
