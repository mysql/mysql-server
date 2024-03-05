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

#ifndef METADATA_CACHE_PLUGIN_CONFIG_INCLUDED
#define METADATA_CACHE_PLUGIN_CONFIG_INCLUDED

#include "mysqlrouter/metadata_cache.h"

#include "mysqlrouter/metadata_cache_plugin_export.h"

#include <chrono>
#include <map>
#include <string>
#include <vector>

#include "mysql/harness/config_option.h"
#include "mysql/harness/config_parser.h"
#include "mysql/harness/plugin.h"
#include "mysql/harness/plugin_config.h"
#include "mysqlrouter/cluster_metadata_dynamic_state.h"
#include "router_options.h"
#include "tcp_address.h"

extern "C" {
extern mysql_harness::Plugin METADATA_CACHE_PLUGIN_EXPORT
    harness_plugin_metadata_cache;
}

class METADATA_CACHE_PLUGIN_EXPORT MetadataCachePluginConfig final
    : public mysql_harness::BasePluginConfig {
 public:
  /** @brief Constructor
   *
   * @param section from configuration file provided as ConfigSection
   */
  MetadataCachePluginConfig(const mysql_harness::ConfigSection *section);

  /**
   * @param option name of the option
   */
  std::string get_default(std::string_view option) const override;
  bool is_required(std::string_view option) const override;

  mutable std::unique_ptr<ClusterMetadataDynamicState>
      metadata_cache_dynamic_state;
  /** @brief MySQL Metadata hosts to connect with */
  const metadata_cache::metadata_servers_list_t metadata_servers_addresses;
  /** @brief User used for authenticating with MySQL Metadata */
  std::string user;
  /** @brief TTL used for storing data in the cache */
  std::chrono::milliseconds ttl;
  /** @brief TTL used for limiting the lifetime of the rest user authentication
   * data stored in the metadata */
  std::chrono::milliseconds auth_cache_ttl;
  /** @brief Refresh rate of the rest user authentication data stored in the
   * cache */
  std::chrono::milliseconds auth_cache_refresh_interval;
  /** @brief Name of the Cluster this Router instance was bootstrapped to use.
   */
  std::string cluster_name;
  /** @brief connect_timeout The time in seconds after which trying to connect
   * to metadata server timeouts */
  unsigned int connect_timeout;
  /** @brief read_timeout The time in seconds after which read from metadata
   * server timeouts */
  unsigned int read_timeout;
  /** @brief memory in kilobytes allocated for thread's stack */
  unsigned int thread_stack_size;
  /** @brief  Whether we should listen to GR notifications from the cluster
   * nodes. */
  bool use_gr_notifications;
  /** @brief  Type of the cluster this configuration was bootstrap against. */
  mysqlrouter::ClusterType cluster_type;
  /** @brief  Id of the router in the metadata. */
  unsigned int router_id;
  /** @brief  SSL settings for metadata cache connection. */
  mysqlrouter::SSLOptions ssl_options;

  // options configured in the metadata
  std::string target_cluster;
  mysqlrouter::TargetCluster::InvalidatedClusterRoutingPolicy
      invalidated_cluster_policy{kDefautlInvalidatedClusterRoutingPolicy};
  bool use_replica_primary_as_rw{false};
  QuorumConnectionLostAllowTraffic unreachable_quorum_allowed_traffic{
      kDefaultQuorumConnectionLostAllowTraffic};
  std::chrono::seconds stats_updates_frequency{std::chrono::seconds(-1)};
  ReadOnlyTargets read_only_targets{kDefaultReadOnlyTargets};

  /** @brief Gets (Group Replication ID for GR cluster or cluster_id for
   * ReplicaSet cluster) if preset in the dynamic configuration.
   *
   * @note  If there is no dynamic configuration (backward compatibility) it
   * returns empty string.
   */
  std::string get_cluster_type_specific_id() const;

  std::string get_clusterset_id() const;

  /** @brief Gets last know ReplicaSet cluster metadata view_id stored in the
   * dynamic state file . */
  uint64_t get_view_id() const;

  void expose_configuration(const mysql_harness::ConfigSection &default_section,
                            const bool initial) const;

 private:
  /** @brief Gets a list of metadata servers.
   *
   *
   * Throws std::invalid_argument on errors.
   *
   * @param default_port Use this port when none was provided
   * @return std::vector<mysql_harness::TCPAddress>
   */
  std::vector<mysql_harness::TCPAddress> get_metadata_servers(
      uint16_t default_port) const;

  mysqlrouter::ClusterType get_cluster_type(
      const mysql_harness::ConfigSection *section);

  std::unique_ptr<ClusterMetadataDynamicState> get_dynamic_state(
      const mysql_harness::ConfigSection *section);
};

#endif  // METADATA_CACHE_PLUGIN_CONFIG_INCLUDED
