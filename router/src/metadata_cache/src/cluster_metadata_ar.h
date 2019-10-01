/*
  Copyright (c) 2019, Oracle and/or its affiliates. All rights reserved.

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

#ifndef METADATA_CACHE_CLUSTER_METADATA_AR_INCLUDED
#define METADATA_CACHE_CLUSTER_METADATA_AR_INCLUDED

#include "cluster_metadata.h"
#include "gr_notifications_listener.h"

/** @class ARClusterMetadata
 *
 * The `ARClusterMetadata` class encapsulates a connection to the Async
 * Replicaset Cluster metadata server.
 *
 */
class METADATA_API ARClusterMetadata : public ClusterMetadata {
 public:
  /** @brief Constructor
   *
   * @param user The user name used to authenticate to the metadata server.
   * @param password The password used to authenticate to the metadata server.
   * @param connect_timeout The time after which trying to connect to the
   *                        metadata server should timeout (in seconds).
   * @param read_timeout The time after which read from metadata server should
   *                     timeout (in seconds).
   * @param connection_attempts The number of times a connection to metadata
   *                            must be attempted, when a connection attempt
   *                            fails.  NOTE: not used so far
   * @param ssl_options SSL related options to use for MySQL connections
   * @param view_id last known view_id of the cluster metadata
   */
  ARClusterMetadata(const std::string &user, const std::string &password,
                    int connect_timeout, int read_timeout,
                    int connection_attempts,
                    const mysqlrouter::SSLOptions &ssl_options,
                    unsigned view_id)
      : ClusterMetadata(user, password, connect_timeout, read_timeout,
                        connection_attempts, ssl_options),
        view_id_(view_id) {}

  explicit ARClusterMetadata(const ARClusterMetadata &) = delete;
  ARClusterMetadata &operator=(const ARClusterMetadata &) = delete;

  /** @brief Destructor
   *
   * Disconnect and release the connection to the metadata node.
   */
  ~ARClusterMetadata() override;

  /** @brief Returns replicasets defined in the metadata server
   *
   * Only to satisfy the API, not used for the Async Replicaset cluster
   *
   * @throws logic_error
   */
  ReplicaSetsByName fetch_instances(
      const std::string & /*cluster_name*/,
      const std::string & /*cluster_type_specific_id*/) override {
    throw std::logic_error("Call to unexpected fetch_instances overload");
  }

  /** @brief Returns replicasets defined in the metadata given set of the
   *         metadata servers (cluster members)
   *
   * @param instances  set of the metadata servers to use to fetch the metadata
   * @param cluster_type_specific_id  (GR ID for GR cluster, cluster_id for AR
   * cluster)
   * @param [out] instance_id of the server the metadata was fetched from
   * @return Map of replicaset ID, server list pairs.
   * @throws metadata_cache::metadata_error
   */
  virtual ReplicaSetsByName fetch_instances(
      const std::vector<metadata_cache::ManagedInstance> &instances,
      const std::string &cluster_type_specific_id,
      std::size_t &instance_id) override;

  /** @brief Returns cluster type this object is suppsed to handle
   */
  mysqlrouter::ClusterType get_cluster_type() override {
    return mysqlrouter::ClusterType::AR_V2;
  }

  void setup_notifications_listener(
      const std::vector<metadata_cache::ManagedInstance> & /*instances*/,
      const GRNotificationListener::NotificationClb & /*callback*/) override {}

  /** @brief Deinitializes the notifications listener thread
   */
  void shutdown_notifications_listener() override {}

 private:
  /** @brief Returns vector of the cluster members according to the metadata of
   * the given metadata server.
   *
   * @param session active connection to the member that is checked for the
   * metadata
   * @param cluster_id ID of the cluster this operation refers to
   * @return vector of the cluster members
   */
  std::vector<metadata_cache::ManagedInstance> fetch_instances_from_member(
      mysqlrouter::MySQLSession &session, const std::string &cluster_id = "");

  /** @brief Returns metadata view id the given member holds
   *
   * @param session active connection to the member that is checked for the view
   * id
   * @param cluster_id ID of the cluster this operation refers to
   * @param result [output parameter [out] member's metadata view_id
   * @return True on success, false otherwise.
   */
  bool get_member_view_id(mysqlrouter::MySQLSession &session,
                          const std::string &cluster_id, unsigned &result);

  unsigned view_id_;
};

#endif  // METADATA_CACHE_CLUSTER_METADATA_AR_INCLUDED
