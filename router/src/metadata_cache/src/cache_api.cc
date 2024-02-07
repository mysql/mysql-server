/*
  Copyright (c) 2016, 2024, Oracle and/or its affiliates.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2.0,
  as published by the Free Software Foundation.

  This program is designed to work with certain software (including
  but not limited to OpenSSL) that is licensed under separate terms,
  as designated in a particular file or component or in included license
  documentation.  The authors of MySQL hereby grant you an additional
  permission to link the program and your derivative works with the
  separately licensed software that they have either included with
  the program or referenced in the documentation.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include <chrono>
#include <map>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>

#include "metadata_cache_ar.h"
#include "metadata_cache_gr.h"
#include "mysqlrouter/metadata_cache.h"

#include "cluster_metadata.h"

// routing's destination_* and the metadata-cache plugin itself
// may work on the cache in parallel.
static std::mutex g_metadata_cache_m;
static std::unique_ptr<MetadataCache> g_metadata_cache(nullptr);

namespace metadata_cache {

ClusterStateListenerInterface::~ClusterStateListenerInterface() = default;
ClusterStateNotifierInterface::~ClusterStateNotifierInterface() = default;

MetadataCacheAPIBase *MetadataCacheAPI::instance() {
  static MetadataCacheAPI instance_;
  return &instance_;
}

#define LOCK_METADATA_AND_CHECK_INITIALIZED()                   \
  std::lock_guard<std::mutex> lock(g_metadata_cache_m);         \
  if (g_metadata_cache == nullptr) {                            \
    throw std::runtime_error("Metadata Cache not initialized"); \
  }

/**
 * Initialize the metadata cache.
 *
 * @param cluster_type type of the cluster the metadata cache object will
 * represent (GR or ReplicaSet)
 * @param router_id id of the router in the cluster metadata
 * @param clusterset_id UUID of the ClusterSet the Cluster belongs to (if
 * bootstrapped as a ClusterSet, empty otherwise)
 * @param metadata_servers The list of cluster metadata servers
 * @param ttl_config metadata TTL configuration
 * @param ssl_options SSL related options for connections
 * @param target_cluster object identifying the Cluster this operation refers to
 * @param session_config Metadata MySQL session configuration
 * @param router_attributes Router attributes to be registered in the metadata
 * @param thread_stack_size memory in kilobytes allocated for thread's stack
 * @param use_cluster_notifications Flag indicating if the metadata cache should
 *                             use cluster notifications as an additional
 *                             trigger for metadata refresh (only available for
 *                             GR cluster type)
 * @param view_id last known view_id of the cluster metadata (only relevant
 *                for ReplicaSet cluster)
 */
void MetadataCacheAPI::cache_init(
    const mysqlrouter::ClusterType cluster_type, const unsigned router_id,
    const std::string &clusterset_id,
    const metadata_servers_list_t &metadata_servers,
    const MetadataCacheTTLConfig &ttl_config,
    const mysqlrouter::SSLOptions &ssl_options,
    const mysqlrouter::TargetCluster &target_cluster,
    const MetadataCacheMySQLSessionConfig &session_config,
    const metadata_cache::RouterAttributes &router_attributes,
    size_t thread_stack_size, bool use_cluster_notifications,
    const uint64_t view_id) {
  std::lock_guard<std::mutex> lock(g_metadata_cache_m);

  switch (cluster_type) {
    case mysqlrouter::ClusterType::RS_V2:
      g_metadata_cache = std::make_unique<ARMetadataCache>(
          router_id, metadata_servers,
          instance_factory_(cluster_type, session_config, ssl_options,
                            use_cluster_notifications, view_id),
          ttl_config, ssl_options, target_cluster, router_attributes,
          thread_stack_size);
      break;
    default:
      g_metadata_cache = std::make_unique<GRMetadataCache>(
          router_id, clusterset_id, metadata_servers,
          instance_factory_(cluster_type, session_config, ssl_options,
                            use_cluster_notifications, view_id),
          ttl_config, ssl_options, target_cluster, router_attributes,
          thread_stack_size, use_cluster_notifications);
  }

  is_initialized_ = true;
}

std::string MetadataCacheAPI::instance_name() const {
  // read by rest_api
  return inst_([](auto &inst) { return inst.name; });
}

void MetadataCacheAPI::instance_name(const std::string &inst_name) {
  // set by metadata_cache_plugin's start()
  return inst_([&inst_name](auto &inst) { inst.name = inst_name; });
}

mysqlrouter::TargetCluster MetadataCacheAPI::target_cluster() const {
  return g_metadata_cache->target_cluster();
}

std::chrono::milliseconds MetadataCacheAPI::ttl() const {
  return g_metadata_cache->ttl();
}

mysqlrouter::ClusterType MetadataCacheAPI::cluster_type() const {
  LOCK_METADATA_AND_CHECK_INITIALIZED();
  return g_metadata_cache->cluster_type();
}

/**
 * Start the metadata cache
 */
void MetadataCacheAPI::cache_start() {
  LOCK_METADATA_AND_CHECK_INITIALIZED();
  g_metadata_cache->start();
}

/**
 * Teardown the metadata cache
 */
void MetadataCacheAPI::cache_stop() noexcept {
  // cache_init() and cache_start() SHOULD BE called from the same thread.
  // That allows for imporant assumptions here:
  // 1) if g_metadata_cache is not nullptr it is fully constructed and
  //    initialized
  // 2) concurrent call to cache_start() is not possible
  // That allows us not to lock g_metadata_cache_m here, which would not be a
  // great idea since stop() is pretty heavy, it notifies the refresh thread and
  // waits for it to finish (plus locks some mutexes internally)
  if (g_metadata_cache)  // might be nullptr if cache_init() failed very early
    g_metadata_cache->stop();
}

/**
 * Lookup the servers that belong to the cluster.
 *
 *
 * @return An object that encapsulates a list of managed MySQL servers.
 *
 */
cluster_nodes_list_t MetadataCacheAPI::get_cluster_nodes() {
  // We only want to keep the lock when checking if the metadata cache global is
  // initialized. The object itself protects its shared state in its
  // replicaset_lookup.
  { LOCK_METADATA_AND_CHECK_INITIALIZED(); }

  return g_metadata_cache->get_cluster_nodes();
}

ClusterTopology MetadataCacheAPI::get_cluster_topology() {
  { LOCK_METADATA_AND_CHECK_INITIALIZED(); }

  return g_metadata_cache->get_cluster_topology();
}

bool MetadataCacheAPI::wait_primary_failover(
    const std::string &primary_server_uuid,
    const std::chrono::seconds &timeout) {
  { LOCK_METADATA_AND_CHECK_INITIALIZED(); }

  return g_metadata_cache->wait_primary_failover(primary_server_uuid, timeout);
}

void MetadataCacheAPI::add_state_listener(
    ClusterStateListenerInterface *listener) {
  // We only want to keep the lock when checking if the metadata cache global is
  // initialized. The object itself protects its shared state in its
  // add_state_listener.
  { LOCK_METADATA_AND_CHECK_INITIALIZED(); }
  g_metadata_cache->add_state_listener(listener);
}

void MetadataCacheAPI::remove_state_listener(
    ClusterStateListenerInterface *listener) {
  // We only want to keep the lock when checking if the metadata cache global is
  // initialized. The object itself protects its shared state in its
  // remove_state_listener.
  { LOCK_METADATA_AND_CHECK_INITIALIZED(); }
  g_metadata_cache->remove_state_listener(listener);
}

void MetadataCacheAPI::add_acceptor_handler_listener(
    AcceptorUpdateHandlerInterface *listener) {
  // We only want to keep the lock when checking if the metadata cache global is
  // initialized. The object itself protects its shared state in its
  // add_acceptor_handler_listener.
  { LOCK_METADATA_AND_CHECK_INITIALIZED(); }
  g_metadata_cache->add_acceptor_handler_listener(listener);
}

void MetadataCacheAPI::remove_acceptor_handler_listener(
    AcceptorUpdateHandlerInterface *listener) {
  // We only want to keep the lock when checking if the metadata cache global is
  // initialized. The object itself protects its shared state in its
  // remove_acceptor_handler_listener.
  { LOCK_METADATA_AND_CHECK_INITIALIZED(); }
  g_metadata_cache->remove_acceptor_handler_listener(listener);
}

void MetadataCacheAPI::add_md_refresh_listener(
    MetadataRefreshListenerInterface *listener) {
  // We only want to keep the lock when checking if the metadata cache global is
  // initialized. The object itself protects its shared state in its
  // add_md_refresh_listener.
  { LOCK_METADATA_AND_CHECK_INITIALIZED(); }
  g_metadata_cache->add_md_refresh_listener(listener);
}

void MetadataCacheAPI::remove_md_refresh_listener(
    MetadataRefreshListenerInterface *listener) {
  // We only want to keep the lock when checking if the metadata cache global is
  // initialized. The object itself protects its shared state in its
  // remove_md_refresh_listener.
  { LOCK_METADATA_AND_CHECK_INITIALIZED(); }
  g_metadata_cache->remove_md_refresh_listener(listener);
}

MetadataCacheAPI::RefreshStatus MetadataCacheAPI::get_refresh_status() {
  LOCK_METADATA_AND_CHECK_INITIALIZED();

  return g_metadata_cache->refresh_status();
}

std::pair<bool, MetaData::auth_credentials_t::mapped_type>
MetadataCacheAPI::get_rest_user_auth_data(const std::string &user) const {
  LOCK_METADATA_AND_CHECK_INITIALIZED();
  return g_metadata_cache->get_rest_user_auth_data(user);
}

void MetadataCacheAPI::enable_fetch_auth_metadata() {
  LOCK_METADATA_AND_CHECK_INITIALIZED();
  return g_metadata_cache->enable_fetch_auth_metadata();
}

void MetadataCacheAPI::force_cache_update() {
  LOCK_METADATA_AND_CHECK_INITIALIZED();
  return g_metadata_cache->force_cache_update();
}

void MetadataCacheAPI::check_auth_metadata_timers() const {
  LOCK_METADATA_AND_CHECK_INITIALIZED();
  return g_metadata_cache->check_auth_metadata_timers();
}

void MetadataCacheAPI::handle_sockets_acceptors_on_md_refresh() {
  LOCK_METADATA_AND_CHECK_INITIALIZED();

  g_metadata_cache->handle_sockets_acceptors_on_md_refresh();
}

bool MetadataCacheAPI::fetch_whole_topology() const {
  LOCK_METADATA_AND_CHECK_INITIALIZED();

  return g_metadata_cache->fetch_whole_topology();
}

void MetadataCacheAPI::fetch_whole_topology(bool val) {
  LOCK_METADATA_AND_CHECK_INITIALIZED();

  g_metadata_cache->fetch_whole_topology(val);
}

}  // namespace metadata_cache
