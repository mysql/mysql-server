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

#include "metadata_cache_ar.h"

#include "mysql/harness/logging/logging.h"

IMPORT_LOG_FUNCTIONS()

bool ARMetadataCache::refresh() {
  bool changed{false};
  bool fetched{false};
  unsigned view_id{0};

  size_t metadata_server_id;
  auto cluster_data_temp = meta_data_->fetch_instances(
      metadata_servers_, cluster_type_specific_id_, metadata_server_id);

  {
    // Ensure that the refresh does not result in an inconsistency during the
    // lookup.
    std::lock_guard<std::mutex> lock(cache_refreshing_mutex_);
    fetched = !cluster_data_temp.empty();
    if (fetched && (cluster_data_ != cluster_data_temp)) {
      cluster_data_ = cluster_data_temp;
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
        target_cluster_.c_str());
    // dump some debugging info about the cluster
    if (cluster_data_.empty())
      log_error("Metadata for cluster '%s' is empty!", target_cluster_.c_str());
    else {
      view_id = cluster_data_.view_id;
      log_info("view_id = %u, (%i members)", view_id,
               (int)cluster_data_.members.size());
      for (auto &mi : cluster_data_.members) {
        log_info("    %s:%i / %i - mode=%s %s", mi.host.c_str(), mi.port,
                 mi.xport, to_string(mi.mode).c_str(),
                 get_hidden_info(mi).c_str());

        if (mi.mode == metadata_cache::ServerMode::ReadWrite) {
          has_unreachable_nodes = false;
        }
      }
    }

    on_instances_changed(/*md_servers_reachable=*/true, view_id);

    auto metadata_servers_tmp = get_cluster_nodes();
    // never let the list that we iterate over become empty as we would
    // not recover from that
    on_refresh_succeeded(metadata_servers_[metadata_server_id]);
    if (!metadata_servers_tmp.empty()) {
      metadata_servers_ = std::move(metadata_servers_tmp);
    }
  } else if (trigger_acceptor_update_on_next_refresh_) {
    // Instances information has not changed, but we failed to start listening
    // on incoming sockets, therefore we must retry on next metadata refresh.
    on_handle_sockets_acceptors();
  }

  return true;
}
