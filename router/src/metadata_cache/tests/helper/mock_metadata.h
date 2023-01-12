/*
  Copyright (c) 2016, 2023, Oracle and/or its affiliates.

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
#ifndef MOCK_METADATA_INCLUDED
#define MOCK_METADATA_INCLUDED

#include <vector>

#include "cluster_metadata_gr.h"

#include "tcp_address.h"

/** @class MockNG
 *
 * Used for simulating NG metadata for testing purposes.
 *
 */

class MockNG : public GRClusterMetadata {
 public:
  /**
   * Objects representing the servers that are part of the topology.
   */
  metadata_cache::ManagedInstance ms1{GR};
  metadata_cache::ManagedInstance ms2{GR};
  metadata_cache::ManagedInstance ms3{GR};

  /**
   * Server list for the cluster. Each server object
   * represents all relevant information about the server that is part of the
   * topology.
   */
  metadata_cache::cluster_nodes_list_t cluster_instances_vector;

  /**
   * The information about the HA topology being managed.
   */
  metadata_cache::ClusterTopology cluster_topology;

  metadata_cache::metadata_servers_list_t metadata_servers;

  /** @brief Constructor
   * @param session_config Metadata MySQL session configuration
   * @param use_cluster_notifications Flag indicating if the metadata cache
   *                                  should use cluster notifications as an
   *                                  additional trigger for metadata refresh
   *                                  (only available for GR cluster type)
   */
  MockNG(const metadata_cache::MetadataCacheMySQLSessionConfig &session_config,
         const mysqlrouter::SSLOptions &ssl_options = mysqlrouter::SSLOptions(),
         const bool use_cluster_notifications = false);

  /** @brief Destructor
   *
   * Disconnect and release the connection to the metadata node.
   */
  ~MockNG() override;

  /** @brief Mock connect method.
   *
   * Mock connect method, does nothing.
   *
   * @return a boolean to indicate if the connection was successful.
   */
  bool connect_and_setup_session(const metadata_cache::metadata_server_t
                                     &metadata_server) noexcept override;

  /** @brief Mock disconnect method.
   *
   * Mock method, does nothing.
   *
   */
  void disconnect() noexcept override;

  /**
   *
   * Returns cluster topology object.
   *
   * @return Cluster topology object.
   */
  stdx::expected<metadata_cache::ClusterTopology, std::error_code>
  fetch_cluster_topology(
      const std::atomic<bool> & /*terminated*/,
      mysqlrouter::TargetCluster &target_cluster, const unsigned /*router_id*/,
      const metadata_cache::metadata_servers_list_t &metadata_servers,
      bool needs_writable_node, const std::string &clusterset_id,
      bool whole_topology, size_t &instance_id) override;

#if 0  // not used so far
  /**
   *
   * Returns a mock refresh interval.
   *
   * @return refresh interval of the Metadata cache.
   */
  unsigned int fetch_ttl() override;
#endif
 private:
  static constexpr auto GR = metadata_cache::InstanceType::GroupMember;
};

#endif  // MOCK_METADATA_INCLUDED
