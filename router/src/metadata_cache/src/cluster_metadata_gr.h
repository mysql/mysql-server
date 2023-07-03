/*
  Copyright (c) 2019, 2022, Oracle and/or its affiliates.

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

#ifndef METADATA_CACHE_CLUSTER_METADATA_GR_INCLUDED
#define METADATA_CACHE_CLUSTER_METADATA_GR_INCLUDED

#include "mysqlrouter/metadata_cache_export.h"

#include "cluster_metadata.h"
#include "gr_notifications_listener.h"

struct GroupReplicationMember;
class GRMetadataBackend;

enum class GRClusterStatus {
  AvailableWritable,
  AvailableReadOnly,
  UnavailableRecovering,
  Unavailable
};

class METADATA_CACHE_EXPORT GRClusterMetadata : public ClusterMetadata {
 public:
  /** @brief Constructor
   *
   * @param session_config Metadata MySQL session configuration
   * @param ssl_options SSL related options to use for MySQL connections
   * @param use_cluster_notifications Flag indicating if the metadata cache
   * should use cluster notifications as an additional trigger for metadata
   * refresh
   */
  GRClusterMetadata(
      const metadata_cache::MetadataCacheMySQLSessionConfig &session_config,
      const mysqlrouter::SSLOptions &ssl_options,
      const bool use_cluster_notifications = false);

  explicit GRClusterMetadata(const GRClusterMetadata &) = delete;
  GRClusterMetadata &operator=(const GRClusterMetadata &) = delete;

  /** @brief Destructor
   *
   * Disconnect and release the connection to the metadata node.
   */
  ~GRClusterMetadata() override;

  /** @brief Returns cluster defined in the metadata given set of the
   *         metadata servers (cluster members)
   *
   * @param terminated flag indicating that the process is cterminating,
   * allowing the function to leave earlier if possible
   * @param [in,out] target_cluster object identifying the Cluster this
   * operation refers to
   * @param router_id id of the router in the cluster metadata
   * @param metadata_servers  set of the metadata servers to use to fetch the
   * metadata
   * @param needs_writable_node flag indicating if the caller needs us to query
   * for writable node
   * @param group_name Cluster Replication Group name (if bootstrapped as a
   * single Cluster)
   * @param clusterset_id UUID of the ClusterSet the Cluster belongs to (if
   * bootstrapped as a ClusterSet)
   * @param whole_topology return all usable nodes, ignore potential metadata
   * filters or policies (like target_cluster etc.)
   * @param [out] instance_id of the server the metadata was fetched from
   * @return object containing cluster topology information in case of success,
   * or error code in case of failure
   * @throws metadata_cache::metadata_error
   */
  stdx::expected<metadata_cache::ClusterTopology, std::error_code>
  fetch_cluster_topology(
      const std::atomic<bool> &terminated,
      mysqlrouter::TargetCluster &target_cluster, const unsigned router_id,
      const metadata_cache::metadata_servers_list_t &metadata_servers,
      bool needs_writable_node, const std::string &group_name,
      const std::string &clusterset_id, bool whole_topology,
      std::size_t &instance_id) override;

  /** @brief Initializes the notifications listener thread (if a given cluster
   * type supports it)
   *
   * @param cluster_topology current topology of the monitored Cluster(s)
   * to
   * @param callback  callback function to get called when the GR notification
   *                  was received
   */
  void setup_notifications_listener(
      const metadata_cache::ClusterTopology &cluster_topology,
      const GRNotificationListener::NotificationClb &callback) override {
    if (gr_notifications_listener_)
      gr_notifications_listener_->setup(cluster_topology, callback);
  }

  /** @brief Deinitializes the notifications listener thread
   */
  void shutdown_notifications_listener() override {
    gr_notifications_listener_.reset();
  }

  /** @brief Returns cluster type this object is suppsed to handle
   */
  mysqlrouter::ClusterType get_cluster_type() override;

  /** @brief Get authentication data of the rest users from the metadata.
   *
   * Authentication data is stored in the router_rest_accounts table. This
   * method fetches the following information: username, password hash,
   * privileges and name of the authentication mechanism that should be used.
   *
   * @param target_cluster information about the Cluster that this information
   * is retrieved for
   * @param cluster_type_specific_id additional information about the Cluster
   * that this information is retrieved for (clusterset_id in case of
   * clusterset)
   *
   * @returns authentication data of the rest users stored in the metadata
   */
  auth_credentials_t fetch_auth_credentials(
      const mysqlrouter::TargetCluster &target_cluster,
      const std::string &cluster_type_specific_id) override;

  std::optional<std::chrono::seconds>
  get_periodic_stats_update_frequency() noexcept override;

 protected:
  /** @brief Queries the metadata server for the list of instances that belong
   * to the desired cluster.
   */
  metadata_cache::ClusterTopology fetch_instances_from_metadata_server(
      const mysqlrouter::TargetCluster &target_cluster,
      const std::string &cluster_type_specific_id);

  /** Query the GR performance_schema tables for live information about a
   * cluster.
   *
   * update_cluster_status() calls check_cluster_status() for some of its
   * processing. Together, they:
   * - check current topology (status) returned from a cluster node
   * - update 'instances' with this state
   * - get other metadata about the cluster
   *
   * The information is pulled from GR maintained performance_schema tables.
   */
  void update_cluster_status(metadata_cache::ManagedCluster &cluster);

  GRClusterStatus check_cluster_status(
      std::vector<metadata_cache::ManagedInstance> &instances,
      const std::map<std::string, GroupReplicationMember> &member_status,
      bool &metadata_gr_discrepancy) const noexcept;

  void reset_metadata_backend(const mysqlrouter::ClusterType type);
  std::unique_ptr<GRMetadataBackend> metadata_backend_;

 private:
  void update_backend(const mysqlrouter::MetadataSchemaVersion &version,
                      unsigned int router_id);

  std::unique_ptr<GRNotificationListener> gr_notifications_listener_;

  friend class GRMetadataBackend;
  friend class GRClusterSetMetadataBackend;

#ifdef FRIEND_TEST
  FRIEND_TEST(MetadataTest, FetchInstancesFromMetadataServer);
  FRIEND_TEST(MetadataTest,
              UpdateClusterStatus_PrimaryMember_FailConnectOnNode2);
  FRIEND_TEST(MetadataTest,
              UpdateClusterStatus_PrimaryMember_FailConnectOnAllNodes);
  FRIEND_TEST(MetadataTest, UpdateClusterStatus_PrimaryMember_FailQueryOnNode1);
  FRIEND_TEST(MetadataTest,
              UpdateClusterStatus_PrimaryMember_FailQueryOnAllNodes);
  FRIEND_TEST(MetadataTest, UpdateClusterStatus_Status_FailQueryOnNode1);
  FRIEND_TEST(MetadataTest, UpdateClusterStatus_Status_FailQueryOnAllNodes);
  FRIEND_TEST(MetadataTest, UpdateClusterStatus_SimpleSunnyDayScenario);
  FRIEND_TEST(MetadataTest, CheckClusterStatus_1Online1RecoveringNotInMetadata);
  FRIEND_TEST(MetadataTest, CheckClusterStatus_3NodeSetup);
  FRIEND_TEST(MetadataTest, CheckClusterStatus_VariableNodeSetup);
  FRIEND_TEST(MetadataTest, CheckClusterStatus_VariousStatuses);
  FRIEND_TEST(MetadataTest, UpdateClusterStatus_PrimaryMember_EmptyOnNode1);
  FRIEND_TEST(MetadataTest, UpdateClusterStatus_PrimaryMember_EmptyOnAllNodes);
  FRIEND_TEST(MetadataTest, CheckClusterStatus_Recovering);
  FRIEND_TEST(MetadataTest, CheckClusterStatus_ErrorAndOther);
  FRIEND_TEST(MetadataTest, CheckClusterStatus_Cornercase2of5Alive);
  FRIEND_TEST(MetadataTest, CheckClusterStatus_Cornercase3of5Alive);
  FRIEND_TEST(MetadataTest, CheckClusterStatus_Cornercase1Common);
#endif
};

#endif  // METADATA_CACHE_CLUSTER_METADATA_GR_INCLUDED
