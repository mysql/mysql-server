/*
  Copyright (c) 2016, 2018, Oracle and/or its affiliates. All rights reserved.

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

#ifndef METADATA_CACHE_METADATA_INCLUDED
#define METADATA_CACHE_METADATA_INCLUDED

#include "metadata.h"
#include "mysqlrouter/metadata_cache.h"
#include "mysqlrouter/mysql_session.h"
#include "tcp_address.h"

#include <string.h>
#include <chrono>
#include <map>
#include <memory>
#include <string>
#include <vector>

struct GroupReplicationMember;

namespace mysqlrouter {
class MySQLSession;
}

/** @class ClusterMetadata
 *
 * The `ClusterMetadata` class encapsulates a connection to the Metadata server.
 * It uses the mysqlrouter::MySQLSession to setup, manage and retrieve results.
 *
 */
class METADATA_API ClusterMetadata : public MetaData {
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
   * @param ttl The time to live of the data in the cache (in milliseconds).
   * @param ssl_options SSL related options to use for MySQL connections
   */
  ClusterMetadata(const std::string &user, const std::string &password,
                  int connect_timeout, int read_timeout,
                  int connection_attempts, std::chrono::milliseconds ttl,
                  const mysqlrouter::SSLOptions &ssl_options);

  /** @brief Destructor
   *
   * Disconnect and release the connection to the metadata node.
   */
  ~ClusterMetadata() override;

  /** @brief Returns replicasets defined in the metadata server
   *
   * Returns relation as a std::map between replicaset name and object
   * of the replicasets defined in the metadata and GR status tables.
   *
   * @param cluster_name the name of the cluster to query
   * @return Map of replicaset ID, server list pairs.
   * @throws metadata_cache::metadata_error
   */
  ReplicaSetsByName fetch_instances(const std::string &cluster_name)
      override;  // throws metadata_cache::metadata_error

#if 0  // not used so far
  /** @brief Returns the refresh interval provided by the metadata server.
   *
   * Returns the refresh interval (also known as TTL) provided by metadata server.
   *
   * @return refresh interval of the Metadata cache.
   */
  unsigned int fetch_ttl() override;
#endif

  /** @brief Connects with the Metadata server
   *
   *
   * @param metadata_server the server instance for which the connection
   *                        should be attempted.
   *
   * @return a boolean to indicate if the connection was successful.
   */
  bool connect(
      const metadata_cache::ManagedInstance &metadata_server) noexcept override;

  /** @brief Disconnects from the Metadata server
   *
   * This is a no-op, as MySQLSession object used underneath for
   * connection handling employs RAII, making this method unnecessary.
   */
  void disconnect() noexcept override {}

 private:
  /** Connects a MYSQL connection to the given instance
   */
  bool do_connect(mysqlrouter::MySQLSession &connection,
                  const metadata_cache::ManagedInstance &mi);

  /** @brief Queries the metadata server for the list of instances and
   * replicasets that belong to the desired cluster.
   */
  ReplicaSetsByName fetch_instances_from_metadata_server(
      const std::string &cluster_name);

  /** Query the GR performance_schema tables for live information about a
   * replicaset.
   *
   * update_replicaset_status() calls check_replicaset_status() for some of its
   * processing. Together, they:
   * - check current topology (status) returned from a replicaset node
   * - update 'instances' with this state
   * - get other metadata about the replicaset
   *
   * The information is pulled from GR maintained performance_schema tables.
   */
  void update_replicaset_status(
      const std::string &name,
      metadata_cache::ManagedReplicaSet
          &replicaset);  // throws metadata_cache::metadata_error

  /** @brief Hard to summarise, please read the full description
   *
   * Does two things based on `member_status` provided:
   * - updates `instances` with status info from `member_status`
   * - performs quorum calculations and returns replicaset's overall health
   *   based on the result, one of: read-only, read-write or not-available
   *
   * @param member_status node statuses obtained from status SQL query
   * @param instances list of nodes to be updated with status info
   * @return replicaset availability state (RW, RO or NA)
   */
  metadata_cache::ReplicasetStatus check_replicaset_status(
      std::vector<metadata_cache::ManagedInstance> &instances,
      const std::map<std::string, GroupReplicationMember> &member_status) const
      noexcept;

  // Metadata node connection information
  std::string user_;
  std::string password_;

  // Metadata node generic information
  std::chrono::milliseconds ttl_;
  mysql_ssl_mode ssl_mode_;
  mysqlrouter::SSLOptions ssl_options_;

  std::string cluster_name_;
#if 0  // not used so far
  std::string metadata_uuid_;
  std::string message_;
#endif

  // The time after which trying to connect to the metadata server should
  // timeout.
  int connect_timeout_;
  // The time after which read from metadata server should timeout.
  int read_timeout_;

#if 0  // not used so far
  // The number of times we should try connecting to the metadata server if a
  // connection attempt fails.
  int connection_attempts_;
#endif

  // connection to metadata server (it may also be shared with GR status queries
  // for optimisation purposes)
  std::shared_ptr<mysqlrouter::MySQLSession> metadata_connection_;

#if 0  // not used so far
  // How many times we tried to reconnected (for logging purposes)
  size_t reconnect_tries_;
#endif

#ifdef FRIEND_TEST
  FRIEND_TEST(MetadataTest, FetchInstancesFromMetadataServer);
  FRIEND_TEST(MetadataTest, CheckReplicasetStatus_3NodeSetup);
  FRIEND_TEST(MetadataTest, CheckReplicasetStatus_VariableNodeSetup);
  FRIEND_TEST(MetadataTest, CheckReplicasetStatus_VariousStatuses);
  FRIEND_TEST(MetadataTest,
              UpdateReplicasetStatus_PrimaryMember_FailConnectOnNode2);
  FRIEND_TEST(MetadataTest,
              UpdateReplicasetStatus_PrimaryMember_FailConnectOnAllNodes);
  FRIEND_TEST(MetadataTest, UpdateReplicasetStatus_PrimaryMember_EmptyOnNode1);
  FRIEND_TEST(MetadataTest,
              UpdateReplicasetStatus_PrimaryMember_EmptyOnAllNodes);
  FRIEND_TEST(MetadataTest,
              UpdateReplicasetStatus_PrimaryMember_FailQueryOnNode1);
  FRIEND_TEST(MetadataTest,
              UpdateReplicasetStatus_PrimaryMember_FailQueryOnAllNodes);
  FRIEND_TEST(MetadataTest, UpdateReplicasetStatus_Status_FailQueryOnNode1);
  FRIEND_TEST(MetadataTest, UpdateReplicasetStatus_Status_FailQueryOnAllNodes);
  FRIEND_TEST(MetadataTest, UpdateReplicasetStatus_SimpleSunnyDayScenario);
  FRIEND_TEST(MetadataTest, CheckReplicasetStatus_Recovering);
  FRIEND_TEST(MetadataTest, CheckReplicasetStatus_ErrorAndOther);
  FRIEND_TEST(MetadataTest, CheckReplicasetStatus_Cornercase2of5Alive);
  FRIEND_TEST(MetadataTest, CheckReplicasetStatus_Cornercase3of5Alive);
  FRIEND_TEST(MetadataTest, CheckReplicasetStatus_Cornercase1Common);
#endif
};

#endif  // METADATA_CACHE_METADATA_INCLUDED
