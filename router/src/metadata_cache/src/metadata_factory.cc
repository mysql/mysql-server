/*
  Copyright (c) 2016, 2022, Oracle and/or its affiliates.

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

#include "metadata_cache.h"

#include "cluster_metadata.h"
#include "cluster_metadata_ar.h"
#include "cluster_metadata_gr.h"

namespace metadata_cache {
static std::shared_ptr<MetaData> meta_data{nullptr};
/**
 * Return an instance of cluster metadata.
 *
 * @param cluster_type type of the cluster the metadata cache object will
 * represent (GR or ReplicaSet)
 * @param session_config Metadata MySQL session configuration
 * @param ssl_options SSL related options to be used for connection
 * @param use_cluster_notifications Flag indicating if the metadata cache
 *                                  should use cluster notifications as an
 *                                  additional trigger for metadata refresh
 *                                  (only available for GR cluster type)
 * @param view_id last known view_id of the cluster metadata (only relevant
 *                for ReplicaSet cluster)
 */
std::shared_ptr<MetaData> metadata_factory_get_instance(
    const mysqlrouter::ClusterType cluster_type,
    const metadata_cache::MetadataCacheMySQLSessionConfig &session_config,
    const mysqlrouter::SSLOptions &ssl_options,
    const bool use_cluster_notifications, const unsigned view_id) {
  switch (cluster_type) {
    case mysqlrouter::ClusterType::RS_V2:
      meta_data = std::make_unique<ARClusterMetadata>(session_config,
                                                      ssl_options, view_id);
      break;
    default:
      meta_data = std::make_unique<GRClusterMetadata>(
          session_config, ssl_options, use_cluster_notifications);
  }

  return meta_data;
}
}  // namespace metadata_cache
