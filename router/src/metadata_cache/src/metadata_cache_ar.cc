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

#include "metadata_cache_ar.h"

#include "mysql/harness/logging/logging.h"

IMPORT_LOG_FUNCTIONS()

bool ARMetadataCache::refresh(bool needs_writable_node) {
  bool changed{false};
  uint64_t view_id{0};

  size_t metadata_server_id;
  const auto res = meta_data_->fetch_cluster_topology(
      terminated_, target_cluster_, router_id_, metadata_servers_,
      needs_writable_node, "", true, metadata_server_id);

  if (!res) {
    const bool md_servers_reachable =
        res.error() !=
            metadata_cache::metadata_errc::no_metadata_server_reached &&
        res.error() !=
            metadata_cache::metadata_errc::no_metadata_read_successful;

    on_refresh_failed(terminated_, md_servers_reachable);
    return false;
  }

  const auto cluster_topology = res.value();

  {
    // Ensure that the refresh does not result in an inconsistency during the
    // lookup.
    std::lock_guard<std::mutex> lock(cache_refreshing_mutex_);
    if (cluster_topology_ != cluster_topology) {
      cluster_topology_ = cluster_topology;
      changed = true;
    } else {
      cluster_topology_.writable_server = cluster_topology.writable_server;
    }
  }

  on_md_refresh(changed, cluster_topology_);

  const auto cluster_members = cluster_topology_.get_all_members();

  if (changed) {
    log_info(
        "Potential changes detected in cluster '%s' after metadata refresh",
        target_cluster_.c_str());
    // dump some debugging info about the cluster
    if (cluster_members.empty())
      log_error("Metadata for cluster '%s' is empty!", target_cluster_.c_str());
    else {
      view_id = cluster_topology_.view_id;
      log_info("view_id = %" PRIu64 ", (%i members)", view_id,
               (int)cluster_members.size());
      for (const auto &mi : cluster_members) {
        log_info("    %s:%i / %i - mode=%s %s", mi.host.c_str(), mi.port,
                 mi.xport, to_string(mi.mode).c_str(),
                 get_hidden_info(mi).c_str());
      }
    }

    on_instances_changed(/*md_servers_reachable=*/true, cluster_topology,
                         view_id);

    on_refresh_succeeded(metadata_servers_[metadata_server_id]);

    // never let the list that we iterate over become empty as we would
    // not recover from that
    if (!cluster_topology.metadata_servers.empty()) {
      metadata_servers_ = std::move(cluster_topology.metadata_servers);
    }
  } else if (trigger_acceptor_update_on_next_refresh_) {
    // Instances information has not changed, but we failed to start listening
    // on incoming sockets, therefore we must retry on next metadata refresh.
    on_handle_sockets_acceptors();
  }

  return true;
}
