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

#include "metadata_cache_ar.h"

#include "mysql/harness/logging/logging.h"

IMPORT_LOG_FUNCTIONS()

bool ARMetadataCache::refresh() {
  bool changed{false};
  bool fetched{false};
  unsigned view_id{0};

  size_t metadata_server_id;
  auto replicaset_data_temp = meta_data_->fetch_instances(
      metadata_servers_, cluster_type_specific_id_, metadata_server_id);

  {
    // Ensure that the refresh does not result in an inconsistency during the
    // lookup.
    std::lock_guard<std::mutex> lock(cache_refreshing_mutex_);
    fetched = !replicaset_data_temp.empty();
    if (fetched && (replicaset_data_ != replicaset_data_temp)) {
      replicaset_data_ = replicaset_data_temp;
      changed = true;
    }
  }

  if (!fetched) {
    on_refresh_failed(/*broke_loop=*/false);
    return false;
  }

  if (changed) {
    log_info(
        "Potential changes detected in cluster '%s' after metadata refresh",
        cluster_name_.c_str());
    // dump some debugging info about the cluster
    if (replicaset_data_.empty())
      log_error("Metadata for cluster '%s' is empty!", cluster_name_.c_str());
    else {
      // there is only one replicaset
      auto &cluster_data = replicaset_data_["default"];
      view_id = cluster_data.view_id;
      log_info("view_id = %u, (%i members)", view_id,
               (int)cluster_data.members.size());
      for (auto &mi : cluster_data.members) {
        log_info("    %s:%i / %i - role=%s mode=%s", mi.host.c_str(), mi.port,
                 mi.xport, mi.role.c_str(), to_string(mi.mode).c_str());

        if (mi.mode == metadata_cache::ServerMode::ReadWrite) {
          std::lock_guard<std::mutex> lock(
              replicasets_with_unreachable_nodes_mtx_);
          auto rs_with_unreachable_node =
              replicasets_with_unreachable_nodes_.find("default");
          if (rs_with_unreachable_node !=
              replicasets_with_unreachable_nodes_.end()) {
            // disable "emergency mode" for this replicaset
            replicasets_with_unreachable_nodes_.erase(rs_with_unreachable_node);
          }
        }
      }
    }

    on_instances_changed(/*md_servers_reachable=*/true, view_id);

    auto metadata_servers_tmp =
        replicaset_lookup(/*cluster_name_ (all clusters)*/ "");
    // never let the list that we iterate over become empty as we would
    // not recover from that
    on_refresh_succeeded(metadata_servers_[metadata_server_id]);
    if (!metadata_servers_tmp.empty()) {
      metadata_servers_ = std::move(metadata_servers_tmp);
    }
  }

  return true;
}
