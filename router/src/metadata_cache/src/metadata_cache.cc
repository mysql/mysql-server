/*
  Copyright (c) 2016, 2019, Oracle and/or its affiliates. All rights reserved.

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

#include <cassert>
#include <cmath>  // fabs()
#include <memory>
#include <stdexcept>
#include <vector>

#include "common.h"
#include "mysql/harness/logging/logging.h"
#include "mysqlrouter/mysql_client_thread_token.h"

IMPORT_LOG_FUNCTIONS()

MetadataCache::MetadataCache(
    const unsigned router_id, const std::string &cluster_type_specifig_id,
    const std::vector<mysql_harness::TCPAddress> &metadata_servers,
    std::shared_ptr<MetaData> cluster_metadata, std::chrono::milliseconds ttl,
    const mysqlrouter::SSLOptions &ssl_options, const std::string &cluster,
    size_t thread_stack_size, bool use_cluster_notifications)
    : cluster_name_(cluster),
      cluster_type_specific_id_(cluster_type_specifig_id),
      ttl_(ttl),
      ssl_options_(ssl_options),
      router_id_(router_id),
      meta_data_(std::move(cluster_metadata)),
      refresh_thread_(thread_stack_size),
      use_cluster_notifications_(use_cluster_notifications) {
  for (const auto &s : metadata_servers) {
    metadata_servers_.emplace_back(s);
  }
}

MetadataCache::~MetadataCache() {
  meta_data_->shutdown_notifications_listener();
}

void *MetadataCache::run_thread(void *context) {
  mysqlrouter::MySQLClientThreadToken api_token;
  MetadataCache *metadata_cache = static_cast<MetadataCache *>(context);

  metadata_cache->refresh_thread();

  return nullptr;
}

// finds first rw instance in the instances vector,
// if not found returns false
static bool find_rw_instance(
    const std::vector<metadata_cache::ManagedInstance> &instances,
    const metadata_cache::ManagedInstance **res_instance) {
  for (auto &instance : instances) {
    if (instance.mode == metadata_cache::ServerMode::ReadWrite) {
      *res_instance = &instance;
      return true;
    }
  }

  return false;
}

void MetadataCache::refresh_thread() {
  mysql_harness::rename_thread("MDC Refresh");
  log_info("Starting metadata cache refresh thread");

  // this will be only useful if the TTL is set to some value that is more than
  // 1 second
  const std::chrono::milliseconds kTerminateOrForcedRefreshCheckInterval =
      std::chrono::seconds(1);

  while (!terminated_) {
    bool refresh_ok{false};
    try {
      refresh_ok = refresh();
    } catch (const mysqlrouter::UpdateInProgressException &) {
      log_info("Cluster metadata in progress, aborting the metada refresh");
    } catch (const std::exception &e) {
      log_info("Failed refreshing metadata: %s", e.what());
      on_refresh_failed(true);
    }

    if (refresh_ok) {
      // we want to update the router version in the routers table once
      // when we start
      if (!version_udpated_) {
        if (!replicaset_data_.empty()) {
          const auto &instances = replicaset_data_.begin()->second.members;
          const metadata_cache::ManagedInstance *rw_instance;
          if (find_rw_instance(instances, &rw_instance)) {
            try {
              meta_data_->update_router_version(*rw_instance, router_id_);
              version_udpated_ = true;
            } catch (const mysqlrouter::UpdateInProgressException &) {
            } catch (...) {
              // we only attempt it once, if it fails we will not try again
              version_udpated_ = true;
            }
          }
        }
      }

      // we want to update the router.last_check_in every 10 ttl queries
      if (last_check_in_udpated_ % 10 == 0) {
        last_check_in_udpated_ = 0;
        if (!replicaset_data_.empty()) {
          const auto &instances = replicaset_data_.begin()->second.members;
          const metadata_cache::ManagedInstance *rw_instance;
          if (find_rw_instance(instances, &rw_instance)) {
            try {
              meta_data_->update_router_last_check_in(*rw_instance, router_id_);
            } catch (const mysqlrouter::UpdateInProgressException &) {
            } catch (...) {
            }
          }
        }
      }
      ++last_check_in_udpated_;
    }

    auto ttl_left = ttl_;
    // wait for up to TTL until next refresh, unless some replicaset loses an
    // online (primary or secondary) server - in that case, "emergency mode" is
    // enabled and we refresh every 1s until "emergency mode" is called off.
    while (ttl_left > std::chrono::milliseconds(0)) {
      auto sleep_for =
          std::min(ttl_left, kTerminateOrForcedRefreshCheckInterval);

      {
        std::unique_lock<std::mutex> lock(refresh_wait_mtx_);
        refresh_wait_.wait_for(lock, sleep_for);
        if (terminated_) return;
        if (refresh_requested_) {
          refresh_requested_ = false;
          break;  // go to the refresh() in the outer loop
        }
      }

      ttl_left -= sleep_for;
      {
        std::lock_guard<std::mutex> lock(
            replicasets_with_unreachable_nodes_mtx_);

        if (!replicasets_with_unreachable_nodes_.empty())
          break;  // we're in "emergency mode", don't wait until TTL expires
      }
    }
  }
}

/**
 * Connect to the metadata servers and refresh the metadata information in the
 * cache.
 */
void MetadataCache::start() {
  // start refresh thread that uses classic protocol
  refresh_thread_.run(&run_thread, this);
}

/**
 * Stop the refresh thread.
 */
void MetadataCache::stop() noexcept {
  {
    std::unique_lock<std::mutex> lk(refresh_wait_mtx_);
    terminated_ = true;
  }
  refresh_wait_.notify_one();
  refresh_thread_.join();
}

/**
 * Return a list of servers that are part of a replicaset.
 *
 * @param replicaset_name The replicaset that is being looked up.
 */
MetadataCache::metadata_servers_list_t MetadataCache::replicaset_lookup(
    const std::string &replicaset_name) {
  std::lock_guard<std::mutex> lock(cache_refreshing_mutex_);
  auto replicaset = (replicaset_name.empty())
                        ? replicaset_data_.begin()
                        : replicaset_data_.find(replicaset_name);

  if (replicaset == replicaset_data_.end()) {
    log_warning("Replicaset '%s' not available", replicaset_name.c_str());
    return {};
  }

  return replicaset->second.members;
}

bool metadata_cache::ManagedInstance::operator==(
    const ManagedInstance &other) const {
  return mysql_server_uuid == other.mysql_server_uuid &&
         replicaset_name == other.replicaset_name && role == other.role &&
         mode == other.mode &&
         std::fabs(weight - other.weight) <
             0.001 &&  // 0.001 = reasonable guess, change if needed
         host == other.host &&
         port == other.port && version_token == other.version_token &&
         xport == other.xport;
}

metadata_cache::ManagedInstance::ManagedInstance(
    const std::string &p_replicaset_name,
    const std::string &p_mysql_server_uuid, const std::string &p_role,
    const ServerMode p_mode, const float p_weight,
    const unsigned int p_version_token, const std::string &p_host,
    const uint16_t p_port, const uint16_t p_xport)
    : replicaset_name(p_replicaset_name),
      mysql_server_uuid(p_mysql_server_uuid),
      role(p_role),
      mode(p_mode),
      weight(p_weight),
      version_token(p_version_token),
      host(p_host),
      port(p_port),
      xport(p_xport) {}

metadata_cache::ManagedInstance::ManagedInstance(const TCPAddress &addr) {
  host = addr.addr == "localhost" ? "127.0.0.1" : addr.addr;
  port = addr.port;
}

metadata_cache::ManagedInstance::operator TCPAddress() const {
  TCPAddress result(host, port);

  return result;
}

bool operator==(const MetaData::ReplicaSetsByName &map_a,
                const MetaData::ReplicaSetsByName &map_b) {
  if (map_a.size() != map_b.size()) return false;
  auto ai = map_a.begin();
  auto bi = map_b.begin();
  for (; ai != map_a.end(); ++ai, ++bi) {
    if ((ai->first != bi->first)) return false;
    // we need to compare 2 vectors if their content is the same
    // but order of their elements can be different as we use
    // SQL with no "ORDER BY" to fetch them from different nodes
    if (ai->second.members.size() != bi->second.members.size()) return false;
    if (ai->second.view_id != bi->second.view_id) return false;
    if (!std::is_permutation(ai->second.members.begin(),
                             ai->second.members.end(),
                             bi->second.members.begin())) {
      return false;
    }
  }

  return true;
}

bool operator!=(const MetaData::ReplicaSetsByName &map_a,
                const MetaData::ReplicaSetsByName &map_b) {
  return !(map_a == map_b);
}

std::string to_string(metadata_cache::ServerMode mode) {
  switch (mode) {
    case metadata_cache::ServerMode::ReadWrite:
      return "RW";
    case metadata_cache::ServerMode::ReadOnly:
      return "RO";
    case metadata_cache::ServerMode::Unavailable:
      return "n/a";
    default:
      return "?";
  }
}

void MetadataCache::on_refresh_failed(bool terminated) {
  refresh_failed_++;
  last_refresh_failed_ = std::chrono::system_clock::now();

  // we failed to fetch metadata from any of the metadata servers
  if (!terminated)
    log_error("Failed fetching metadata from any of the %u metadata servers.",
              static_cast<unsigned>(metadata_servers_.size()));

  // clearing metadata
  {
    bool clearing;
    {
      std::lock_guard<std::mutex> lock(cache_refreshing_mutex_);
      clearing = !replicaset_data_.empty();
      if (clearing) replicaset_data_.clear();
    }
    if (clearing) {
      log_info("... cleared current routing table as a precaution");
      on_instances_changed(/*md_servers_reachable=*/false);
    }
  }
}

void MetadataCache::on_refresh_succeeded(
    const metadata_cache::ManagedInstance &metadata_server) {
  last_refresh_succeeded_ = std::chrono::system_clock::now();
  last_metadata_server_host_ = metadata_server.host;
  last_metadata_server_port_ = metadata_server.port;
  refresh_succeeded_++;
}

void MetadataCache::on_instances_changed(const bool md_servers_reachable,
                                         unsigned view_id) {
  auto instances = replicaset_lookup("" /*cluster_name_*/);
  {
    std::lock_guard<std::mutex> lock(
        replicaset_instances_change_callbacks_mtx_);

    for (auto &replicaset_clb : listeners_) {
      const std::string replicaset_name = replicaset_clb.first;

      for (auto each : listeners_[replicaset_name]) {
        each->notify(instances, md_servers_reachable, view_id);
      }
    }
  }

  if (use_cluster_notifications_) {
    meta_data_->setup_notifications_listener(
        instances, [this]() { on_refresh_requested(); });
  }
}

void MetadataCache::on_refresh_requested() {
  {
    std::unique_lock<std::mutex> lock(refresh_wait_mtx_);
    refresh_requested_ = true;
  }
  refresh_wait_.notify_one();
}

void MetadataCache::mark_instance_reachability(
    const std::string &instance_id, metadata_cache::InstanceStatus status) {
  // If the status is that the primary or secondary instance is physically
  // unreachable, we enable "emergency mode" (temporarily increase the refresh
  // rate to 1/s if currently lower) until the replicaset routing table
  // reflects this reality (or at least that is the the intent; in practice
  // this mechanism is buggy - see Metadata Cache module documentation in
  // Doxygen, section "Emergency mode")

  std::lock_guard<std::mutex> lock(cache_refreshing_mutex_);
  // the replicaset that the given instance belongs to
  metadata_cache::ManagedInstance *instance = nullptr;
  metadata_cache::ManagedReplicaSet *replicaset = nullptr;
  for (auto &rs : replicaset_data_) {
    for (auto &inst : rs.second.members) {
      if (inst.mysql_server_uuid == instance_id) {
        instance = &inst;
        replicaset = &rs.second;
        break;
      }
    }
    if (replicaset) break;
  }

  // If the instance got marked as invalid we want to trigger metadata-cache
  // update ASAP to aviod keeping try to route to that instance
  if (replicaset) {
    std::lock_guard<std::mutex> lplock(replicasets_with_unreachable_nodes_mtx_);
    switch (status) {
      case metadata_cache::InstanceStatus::Reachable:
        break;
      case metadata_cache::InstanceStatus::InvalidHost:
        log_warning(
            "Instance '%s:%i' [%s] of replicaset '%s' is invalid. Increasing "
            "metadata cache refresh frequency.",
            instance->host.c_str(), instance->port, instance_id.c_str(),
            replicaset->name.c_str());
        replicasets_with_unreachable_nodes_.insert(replicaset->name);
        break;
      case metadata_cache::InstanceStatus::Unreachable:
        log_warning(
            "Instance '%s:%i' [%s] of replicaset '%s' is unreachable. "
            "Increasing metadata cache refresh frequency.",
            instance->host.c_str(), instance->port, instance_id.c_str(),
            replicaset->name.c_str());
        replicasets_with_unreachable_nodes_.insert(replicaset->name);
        break;
      case metadata_cache::InstanceStatus::Unusable:
        break;
    }
  }
}

bool MetadataCache::wait_primary_failover(const std::string &replicaset_name,
                                          int timeout) {
  log_debug("Waiting for failover to happen in '%s' for %is",
            replicaset_name.c_str(), timeout);
  time_t stime = std::time(NULL);
  while (std::time(NULL) - stime <= timeout) {
    {
      std::lock_guard<std::mutex> lock(cache_refreshing_mutex_);
      if (replicasets_with_unreachable_nodes_.count(replicaset_name) == 0) {
        return true;
      }
    }
    std::this_thread::sleep_for(metadata_cache::kDefaultMetadataTTL);
  }
  return false;
}

void MetadataCache::add_listener(
    const std::string &replicaset_name,
    metadata_cache::ReplicasetStateListenerInterface *listener) {
  std::lock_guard<std::mutex> lock(replicaset_instances_change_callbacks_mtx_);
  listeners_[replicaset_name].insert(listener);
}

void MetadataCache::remove_listener(
    const std::string &replicaset_name,
    metadata_cache::ReplicasetStateListenerInterface *listener) {
  std::lock_guard<std::mutex> lock(replicaset_instances_change_callbacks_mtx_);
  listeners_[replicaset_name].erase(listener);
}
