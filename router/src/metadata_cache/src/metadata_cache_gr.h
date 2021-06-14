/*
  Copyright (c) 2019, 2021, Oracle and/or its affiliates.

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

#ifndef METADATA_CACHE_METADATA_CACHE_GR_INCLUDED
#define METADATA_CACHE_METADATA_CACHE_GR_INCLUDED

#include "metadata_cache.h"

class METADATA_API GRMetadataCache : public MetadataCache {
 public:
  /**
   * Initialize a connection to the MySQL Metadata server.
   *
   * @param router_id id of the router in the cluster metadata
   * @param group_replication_id id of the replication group
   * @param metadata_servers The servers that store the metadata
   * @param cluster_metadata metadata of the cluster
   * @param ttl The TTL of the cached data
   * @param auth_credentials_ttl TTL of the rest user authentication data
   * @param auth_credentials_refresh_rate Refresh_rate of the rest user
   *                                      authentication data
   * @param ssl_options SSL related options for connection
   * @param target_cluster object identifying the Cluster this operation refers
   * to
   * @param thread_stack_size The maximum memory allocated for thread's stack
   * @param use_gr_notifications Flag indicating if the metadata cache should
   *                             use GR notifications as an additional trigger
   *                             for metadata refresh
   */
  GRMetadataCache(
      const unsigned router_id, const std::string &group_replication_id,
      const std::vector<mysql_harness::TCPAddress> &metadata_servers,
      std::shared_ptr<MetaData> cluster_metadata,
      const std::chrono::milliseconds ttl,
      const std::chrono::milliseconds auth_credentials_ttl,
      const std::chrono::milliseconds auth_credentials_refresh_rate,
      const mysqlrouter::SSLOptions &ssl_options,
      const mysqlrouter::TargetCluster &target_cluster,
      size_t thread_stack_size = mysql_harness::kDefaultStackSizeInKiloBytes,
      bool use_gr_notifications = false)
      : MetadataCache(router_id, group_replication_id, metadata_servers,
                      cluster_metadata, ttl, auth_credentials_ttl,
                      auth_credentials_refresh_rate, ssl_options,
                      target_cluster, thread_stack_size, use_gr_notifications) {
  }

  bool refresh() override;

  mysqlrouter::ClusterType cluster_type() const noexcept override {
    return meta_data_->get_cluster_type();
  }

 private:
#ifdef FRIEND_TEST
  FRIEND_TEST(FailoverTest, basics);
  FRIEND_TEST(FailoverTest, primary_failover);
  FRIEND_TEST(MetadataCacheTest2, basic_test);
  FRIEND_TEST(MetadataCacheTest2, metadata_server_connection_failures);
  friend class MetadataCacheTest;
#endif
};

#endif  // METADATA_CACHE_METADATA_CACHE_GR_INCLUDED
