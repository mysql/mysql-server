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
#include "metadata_factory.h"
#include "mysqlrouter/metadata_cache.h"

#include "cluster_metadata.h"

#include <map>
#include <memory>
#include <stdexcept>

// routing's destination_* and the metadata-cache plugin itself
// may work on the cache in parallel.
static std::mutex g_metadata_cache_m;
static std::unique_ptr<MetadataCache> g_metadata_cache(nullptr);

namespace metadata_cache {

const uint16_t kDefaultMetadataPort = 32275;
const std::chrono::milliseconds kDefaultMetadataTTL =
    std::chrono::milliseconds(500);
const std::string kDefaultMetadataAddress{
    "127.0.0.1:" + mysqlrouter::to_string(kDefaultMetadataPort)};
const std::string kDefaultMetadataUser = "";
const std::string kDefaultMetadataPassword = "";
const std::string kDefaultMetadataCluster =
    "";  // blank cluster name means pick the 1st (and only) cluster
const unsigned int kDefaultConnectTimeout = 30;
const unsigned int kDefaultReadTimeout = 30;

ReplicasetStateListenerInterface::~ReplicasetStateListenerInterface() = default;
ReplicasetStateNotifierInterface::~ReplicasetStateNotifierInterface() = default;

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
 * @param group_replication_id id of the replication group
 * @param metadata_servers The list of cluster metadata servers
 * @param user_credentials The user name and password used to connect to the
 * metadata servers.
 * @param ttl The ttl for the contents of the cache
 * @param ssl_options SSL related options for connections
 * @param cluster_name The name of the cluster from the metadata schema
 * @param connect_timeout The time in seconds after which trying to connect
 *                        to metadata server timeouts
 * @param read_timeout The time in seconds after which read from metadata
 *                     server should timeout.
 * @param thread_stack_size memory in kilobytes allocated for thread's stack
 * @param use_gr_notifications Flag indicating if the metadata cache should
 *                             use GR notifications as an additional trigger
 *                             for metadata refresh
 */
void MetadataCacheAPI::cache_init(
    const std::string &group_replication_id,
    const std::vector<mysql_harness::TCPAddress> &metadata_servers,
    const mysqlrouter::UserCredentials &user_credentials,
    std::chrono::milliseconds ttl, const mysqlrouter::SSLOptions &ssl_options,
    const std::string &cluster_name, int connect_timeout, int read_timeout,
    size_t thread_stack_size, bool use_gr_notifications) {
  std::lock_guard<std::mutex> lock(g_metadata_cache_m);

  g_metadata_cache.reset(new MetadataCache(
      group_replication_id, metadata_servers,
      get_instance(user_credentials.username, user_credentials.password,
                   connect_timeout, read_timeout, 1, ttl, ssl_options,
                   use_gr_notifications),
      ttl, ssl_options, cluster_name, thread_stack_size, use_gr_notifications));

  is_initialized_ = true;
}

std::string MetadataCacheAPI::instance_name() const { return inst_name_; }
void MetadataCacheAPI::instance_name(const std::string &inst_name) {
  inst_name_ = inst_name;
}

std::string MetadataCacheAPI::group_replication_id() const {
  return g_metadata_cache->group_replication_id();
}

std::string MetadataCacheAPI::cluster_name() const {
  return g_metadata_cache->cluster_name();
}

std::chrono::milliseconds MetadataCacheAPI::ttl() const {
  return g_metadata_cache->ttl();
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
  std::lock_guard<std::mutex> lock(g_metadata_cache_m);

  if (g_metadata_cache)  // might be NULL if cache_init() failed very early
    g_metadata_cache->stop();
}

/**
 * Lookup the servers that belong to the given replicaset.
 *
 * @param replicaset_name The name of the replicaset whose servers need
 *                      to be looked up.
 *
 * @return An object that encapsulates a list of managed MySQL servers.
 *
 */
LookupResult MetadataCacheAPI::lookup_replicaset(
    const std::string &replicaset_name) {
  LOCK_METADATA_AND_CHECK_INITIALIZED();

  return LookupResult(g_metadata_cache->replicaset_lookup(replicaset_name));
}

void MetadataCacheAPI::mark_instance_reachability(
    const std::string &instance_id, InstanceStatus status) {
  LOCK_METADATA_AND_CHECK_INITIALIZED();

  g_metadata_cache->mark_instance_reachability(instance_id, status);
}

bool MetadataCacheAPI::wait_primary_failover(const std::string &replicaset_name,
                                             int timeout) {
  LOCK_METADATA_AND_CHECK_INITIALIZED();

  return g_metadata_cache->wait_primary_failover(replicaset_name, timeout);
}

void MetadataCacheAPI::add_listener(
    const std::string &replicaset_name,
    ReplicasetStateListenerInterface *listener) {
  LOCK_METADATA_AND_CHECK_INITIALIZED();
  g_metadata_cache->add_listener(replicaset_name, listener);
}
void MetadataCacheAPI::remove_listener(
    const std::string &replicaset_name,
    ReplicasetStateListenerInterface *listener) {
  LOCK_METADATA_AND_CHECK_INITIALIZED();
  g_metadata_cache->remove_listener(replicaset_name, listener);
}

MetadataCacheAPI::RefreshStatus MetadataCacheAPI::get_refresh_status() {
  LOCK_METADATA_AND_CHECK_INITIALIZED();

  return g_metadata_cache->refresh_status();
}

}  // namespace metadata_cache
