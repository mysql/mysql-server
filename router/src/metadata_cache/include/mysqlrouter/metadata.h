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

#ifndef METADATA_CACHE_METADATA_INTERFACE_INCLUDED
#define METADATA_CACHE_METADATA_INTERFACE_INCLUDED

#include "mysqlrouter/metadata_cache_export.h"

#include <atomic>
#include <chrono>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "my_rapidjson_size_t.h"

#include <rapidjson/document.h>

#include "mysql/harness/stdx/expected.h"
#include "mysqlrouter/cluster_metadata.h"
#include "mysqlrouter/metadata_cache_datatypes.h"
#include "mysqlrouter/mysql_session.h"

/**
 * The metadata class is used to create a pluggable transport layer
 * from which the metadata is fetched for the metadata cache.
 */
class METADATA_CACHE_EXPORT MetaData {
 public:
  using JsonAllocator = rapidjson::CrtAllocator;
  using JsonDocument = rapidjson::Document;
  // username as key, password hash and privileges as value
  using auth_credentials_t =
      std::map<std::string, std::pair<std::string, JsonDocument>>;

  // fetch instances from vector of metadata servers
  virtual stdx::expected<metadata_cache::ClusterTopology, std::error_code>
  fetch_cluster_topology(
      const std::atomic<bool> &terminated,
      mysqlrouter::TargetCluster &target_cluster, const unsigned router_id,
      const metadata_cache::metadata_servers_list_t &metadata_servers,
      bool needs_writable_node, const std::string &clusterset_id,
      bool whole_topology, std::size_t &instance_id) = 0;

  virtual bool update_router_attributes(
      const metadata_cache::metadata_server_t &rw_server,
      const unsigned router_id,
      const metadata_cache::RouterAttributes &router_attributes) = 0;

  virtual bool update_router_last_check_in(
      const metadata_cache::metadata_server_t &rw_server,
      const unsigned router_id) = 0;

  virtual bool connect_and_setup_session(
      const metadata_cache::metadata_server_t &metadata_server) = 0;

  virtual void disconnect() = 0;

  virtual void setup_notifications_listener(
      const metadata_cache::ClusterTopology &cluster_topology,
      const std::function<void()> &callback) = 0;

  virtual void shutdown_notifications_listener() = 0;

  virtual std::shared_ptr<mysqlrouter::MySQLSession> get_connection() = 0;

  virtual mysqlrouter::ClusterType get_cluster_type() = 0;

  virtual auth_credentials_t fetch_auth_credentials(
      const metadata_cache::metadata_server_t &md_server,
      const mysqlrouter::TargetCluster &target_cluster) = 0;

  virtual std::optional<std::chrono::seconds>
  get_periodic_stats_update_frequency() noexcept = 0;

  MetaData() = default;
  // disable copy as it isn't needed right now. Feel free to enable
  // must be explicitly defined though.
  explicit MetaData(const MetaData &) = delete;
  MetaData &operator=(const MetaData &) = delete;
  virtual ~MetaData() = default;
};

#endif  // METADATA_CACHE_METADATA_INTERFACE_INCLUDED
