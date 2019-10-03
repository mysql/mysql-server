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

#ifndef METADATA_CACHE_METADATA_CACHE_AR_INCLUDED
#define METADATA_CACHE_METADATA_CACHE_AR_INCLUDED

#include "metadata_cache.h"

class METADATA_API ARMetadataCache : public MetadataCache {
 public:
  /**
   * Initialize a connection to the MySQL Metadata server.
   *
   * @param router_id id of the router in the cluster metadata
   * @param cluster_id id of the replication group
   * @param metadata_servers The servers that store the metadata
   * @param cluster_metadata metadata of the cluster
   * @param ttl The TTL of the cached data
   * @param ssl_options SSL related options for connection
   * @param cluster_name The name of the desired cluster in the metadata server
   * @param thread_stack_size The maximum memory allocated for thread's stack
   */
  ARMetadataCache(
      const unsigned router_id, const std::string &cluster_id,
      const std::vector<mysql_harness::TCPAddress> &metadata_servers,
      std::shared_ptr<MetaData> cluster_metadata, std::chrono::milliseconds ttl,
      const mysqlrouter::SSLOptions &ssl_options,
      const std::string &cluster_name,
      size_t thread_stack_size = mysql_harness::kDefaultStackSizeInKiloBytes)
      : MetadataCache(router_id, cluster_id, metadata_servers, cluster_metadata,
                      ttl, ssl_options, cluster_name, thread_stack_size,
                      false) {}

  bool refresh() override;
};

#endif  // METADATA_CACHE_METADATA_CACHE_AR_INCLUDED
