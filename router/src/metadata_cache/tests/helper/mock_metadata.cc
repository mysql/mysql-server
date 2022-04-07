/*
  Copyright (c) 2016, 2021, Oracle and/or its affiliates.

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

#include "mock_metadata.h"

#include <map>
#include <memory>
#include <vector>

#include "mysqlrouter/metadata_cache.h"

using namespace std;

MockNG::MockNG(const std::string &user, const std::string &password,
               int connect_timeout, int read_timeout, int connection_attempts,
               const mysqlrouter::SSLOptions &ssl_options,
               const bool use_cluster_notifications)
    : GRClusterMetadata(user, password, connect_timeout, read_timeout,
                        connection_attempts, ssl_options,
                        use_cluster_notifications) {
  ms1.replicaset_name = "replicaset-1";
  ms1.mysql_server_uuid = "instance-1";
  ms1.host = "host-1";
  ms1.port = 3306;
  ms1.xport = 33060;
  ms1.mode = metadata_cache::ServerMode::ReadWrite;

  ms2.replicaset_name = "replicaset-1";
  ms2.mysql_server_uuid = "instance-2";
  ms2.host = "host-2";
  ms2.port = 3306;
  ms2.xport = 33060;
  ms2.mode = metadata_cache::ServerMode::ReadOnly;

  ms3.replicaset_name = "replicaset-1";
  ms3.mysql_server_uuid = "instance-3";
  ms3.host = "host-3";
  ms3.port = 3306;
  ms3.xport = 33060;
  ms3.mode = metadata_cache::ServerMode::ReadOnly;

  ms4.replicaset_name = "replicaset-2";
  ms4.mysql_server_uuid = "instance-4";
  ms4.host = "host-4";
  ms4.port = 3306;
  ms4.xport = 33060;
  ms4.mode = metadata_cache::ServerMode::ReadWrite;

  ms5.replicaset_name = "replicaset-2";
  ms5.mysql_server_uuid = "instance-5";
  ms5.host = "host-5";
  ms5.port = 3306;
  ms5.xport = 33060;
  ms5.mode = metadata_cache::ServerMode::ReadOnly;

  ms6.replicaset_name = "replicaset-2";
  ms6.mysql_server_uuid = "instance-6";
  ms6.host = "host-6";
  ms6.port = 3306;
  ms6.xport = 33060;
  ms6.mode = metadata_cache::ServerMode::ReadOnly;

  ms7.replicaset_name = "replicaset-3";
  ms7.mysql_server_uuid = "instance-7";
  ms7.host = "host-7";
  ms7.port = 3306;
  ms7.xport = 33060;
  ms7.mode = metadata_cache::ServerMode::ReadWrite;

  ms8.replicaset_name = "replicaset-3";
  ms8.mysql_server_uuid = "instance-8";
  ms8.host = "host-8";
  ms8.port = 3306;
  ms8.xport = 33060;
  ms8.mode = metadata_cache::ServerMode::ReadWrite;

  ms9.replicaset_name = "replicaset-3";
  ms9.mysql_server_uuid = "instance-9";
  ms9.host = "host-9";
  ms9.port = 3306;
  ms9.xport = 33060;
  ms9.mode = metadata_cache::ServerMode::ReadWrite;

  replicaset_1_vector.push_back(ms1);
  replicaset_1_vector.push_back(ms2);
  replicaset_1_vector.push_back(ms3);

  replicaset_2_vector.push_back(ms4);
  replicaset_2_vector.push_back(ms5);
  replicaset_2_vector.push_back(ms6);

  replicaset_3_vector.push_back(ms7);
  replicaset_3_vector.push_back(ms8);
  replicaset_3_vector.push_back(ms9);

  replicaset_map["replicaset-1"].name = "replicaset-1";
  replicaset_map["replicaset-1"].single_primary_mode = true;
  replicaset_map["replicaset-1"].members = replicaset_1_vector;

  replicaset_map["replicaset-2"].name = "replicaset-2";
  replicaset_map["replicaset-2"].single_primary_mode = true;
  replicaset_map["replicaset-2"].members = replicaset_2_vector;

  replicaset_map["replicaset-3"].name = "replicaset-3";
  replicaset_map["replicaset-3"].single_primary_mode = false;
  replicaset_map["replicaset-3"].members = replicaset_3_vector;
}

/** @brief Destructor
 *
 * Disconnect and release the connection to the metadata node.
 */
MockNG::~MockNG() = default;

/** @brief Returns relation between replicaset ID and list of servers
 *
 * Returns relation as a std::map between replicaset ID and list of managed
 * servers.
 *
 * @return Map of replicaset ID, server list pairs.
 */
ClusterMetadata::ReplicaSetsByName MockNG::fetch_instances(
    const std::string & /*cluster_name*/,
    const string & /*group_replication_id*/) {
  return replicaset_map;
}

ClusterMetadata::ReplicaSetsByName MockNG::fetch_instances(
    const std::vector<metadata_cache::ManagedInstance> & /*instances*/,
    const string & /*group_replication_id*/, size_t & /*instance_id*/) {
  return replicaset_map;
}

/** @brief Mock connect method.
 *
 * Mock connect method, does nothing.
 *
 * @return a boolean to indicate if the connection was successful.
 */
bool MockNG::connect_and_setup_session(
    const metadata_cache::ManagedInstance &metadata_server) noexcept {
  (void)metadata_server;
  return true;
}

/**
 * Mock disconnect method, does nothing.
 */
void MockNG::disconnect() noexcept {}

#if 0  // not used so far
/**
 *
 * Returns a mock refresh interval.
 *
 * @return refresh interval of the Metadata cache.
 */
unsigned int MockNG::fetch_ttl() {
  return 5;
}
#endif
