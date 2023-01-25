/*
  Copyright (c) 2019, 2023, Oracle and/or its affiliates.

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

#include "mysqlrouter/metadata_cache_export.h"

#include "metadata_cache.h"

class METADATA_CACHE_EXPORT GRMetadataCache : public MetadataCache {
 public:
  /**
   * Initialize a connection to the MySQL Metadata server.
   *
   * @param router_id id of the router in the cluster metadata
   * @param clusterset_id UUID of the ClusterSet the Cluster belongs to (if
   * bootstrapped as a ClusterSet, empty otherwise)
   * @param metadata_servers The servers that store the metadata
   * @param cluster_metadata metadata of the cluster
   * @param ttl_config metadata TTL configuration
   * @param ssl_options SSL related options for connection
   * @param target_cluster object identifying the Cluster this operation refers
   * to
   * @param router_attributes Router attributes to be registered in the metadata
   * @param thread_stack_size The maximum memory allocated for thread's stack
   * @param use_gr_notifications Flag indicating if the metadata cache should
   *                             use GR notifications as an additional trigger
   *                             for metadata refresh
   */
  GRMetadataCache(
      const unsigned router_id, const std::string &clusterset_id,
      const std::vector<mysql_harness::TCPAddress> &metadata_servers,
      std::shared_ptr<MetaData> cluster_metadata,
      const metadata_cache::MetadataCacheTTLConfig &ttl_config,
      const mysqlrouter::SSLOptions &ssl_options,
      const mysqlrouter::TargetCluster &target_cluster,
      const metadata_cache::RouterAttributes &router_attributes,
      size_t thread_stack_size = mysql_harness::kDefaultStackSizeInKiloBytes,
      bool use_gr_notifications = false)
      : MetadataCache(router_id, clusterset_id, metadata_servers,
                      cluster_metadata, ttl_config, ssl_options, target_cluster,
                      router_attributes, thread_stack_size,
                      use_gr_notifications) {}

  bool refresh(bool needs_writable_node) override;

  mysqlrouter::ClusterType cluster_type() const noexcept override {
    return meta_data_->get_cluster_type();
  }

 private:
  void log_cluster_details() const;
};

#endif  // METADATA_CACHE_METADATA_CACHE_GR_INCLUDED
