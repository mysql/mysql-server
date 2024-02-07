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

#ifndef METADATA_CACHE_METADATA_CACHE_INCLUDED
#define METADATA_CACHE_METADATA_CACHE_INCLUDED

#include "mysqlrouter/metadata_cache_export.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <ctime>
#include <future>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <thread>

#include "gr_notifications_listener.h"
#include "mysql/harness/logging/logging.h"
#include "mysql/harness/stdx/monitor.h"
#include "mysql_router_thread.h"
#include "mysqlrouter/metadata.h"
#include "mysqlrouter/metadata_cache.h"

class ClusterMetadata;

/** @class MetadataCache
 *
 * The MetadataCache manages cached information fetched from the
 * MySQL Server.
 *
 */
class METADATA_CACHE_EXPORT MetadataCache
    : public metadata_cache::ClusterStateNotifierInterface {
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
   * @param use_cluster_notifications Flag indicating if the metadata cache
   * should use GR notifications as an additional trigger for metadata refresh
   */
  MetadataCache(
      const unsigned router_id, const std::string &clusterset_id,
      const std::vector<mysql_harness::TCPAddress> &metadata_servers,
      std::shared_ptr<MetaData> cluster_metadata,
      const metadata_cache::MetadataCacheTTLConfig &ttl_config,
      const mysqlrouter::SSLOptions &ssl_options,
      const mysqlrouter::TargetCluster &target_cluster,
      const metadata_cache::RouterAttributes &router_attributes,
      size_t thread_stack_size = mysql_harness::kDefaultStackSizeInKiloBytes,
      bool use_cluster_notifications = false);

  ~MetadataCache() override;

  /** @brief Starts the Metadata Cache
   *
   * Starts the Metadata Cache and launch thread.
   */
  void start();

  /** @brief Stops the Metadata Cache
   *
   * Stops the Metadata Cache and the launch thread.
   */
  void stop() noexcept;

  /** @brief Returns list of managed servers in a cluster
   *
   *
   * @return std::vector containing ManagedInstance objects
   */
  metadata_cache::cluster_nodes_list_t get_cluster_nodes();

  /** @brief Returns object containing current Cluster Topology
   */
  metadata_cache::ClusterTopology get_cluster_topology();

  /** Wait until cluster PRIMARY changes.
   *
   * wait until a change of the PRIMARY is noticed
   *
   * leave early if
   *
   * - 'timeout' expires
   * - process shutdown is requested
   *
   * function has to handle two scenarios:
   *
   * connection to PRIMARY fails because:
   *
   * 1. PRIMARY died and group relects a new member
   * 2. network to PRIMARY lost, but GR sees no fault and PRIMARY does not
   * change.
   *
   * Therefore, if the connection to PRIMARY fails, wait for change of the
   * membership or timeout, whatever happens earlier.
   *
   * @param server_uuid server-uuid of the PRIMARY that we failed to connect
   * @param timeout - amount of time to wait for a failover
   * @return true if a primary member exists
   */
  bool wait_primary_failover(const std::string &server_uuid,
                             const std::chrono::seconds &timeout);

  /** @brief refresh cluster information */
  void refresh_thread();

  /** @brief run refresh thread */
  static void *run_thread(void *context);

  /**
   * @brief Register observer that is notified when there is a change in the
   * cluster nodes setup/state discovered.
   *
   * @param listener Observer object that is notified when cluster nodes
   * state is changed.
   */
  void add_state_listener(
      metadata_cache::ClusterStateListenerInterface *listener) override;

  /**
   * @brief Unregister observer previously registered with add_state_listener()
   *
   * @param listener Observer object that should be unregistered.
   */
  void remove_state_listener(
      metadata_cache::ClusterStateListenerInterface *listener) override;

  /**
   * @brief Register observer that is notified when the state of listening
   * socket acceptors should be updated on the next metadata refresh.
   *
   * @param listener Observer object that is notified when replicaset nodes
   * state is changed.
   */
  void add_acceptor_handler_listener(
      metadata_cache::AcceptorUpdateHandlerInterface *listener);

  /**
   * @brief Unregister observer previously registered with
   * add_acceptor_handler_listener()
   *
   * @param listener Observer object that should be unregistered.
   */
  void remove_acceptor_handler_listener(
      metadata_cache::AcceptorUpdateHandlerInterface *listener);

  /**
   * @brief Register observer that is notified on each metadata refresh event.
   *
   * @param listener Observer object that is notified on md refresh.
   */
  void add_md_refresh_listener(
      metadata_cache::MetadataRefreshListenerInterface *listener);

  /**
   * @brief Unregister observer previously registered with
   * add_md_refresh_listener()
   *
   * @param listener Observer object that should be unregistered.
   */
  void remove_md_refresh_listener(
      metadata_cache::MetadataRefreshListenerInterface *listener);

  metadata_cache::MetadataCacheAPIBase::RefreshStatus refresh_status() {
    return stats_([](auto const &stats)
                      -> metadata_cache::MetadataCacheAPIBase::RefreshStatus {
      return {stats.refresh_failed,
              stats.refresh_succeeded,
              stats.last_refresh_succeeded,
              stats.last_refresh_failed,
              stats.last_metadata_server_host,
              stats.last_metadata_server_port};
    });
  }

  std::chrono::milliseconds ttl() const { return ttl_config_.ttl; }
  mysqlrouter::TargetCluster target_cluster() const { return target_cluster_; }

  virtual mysqlrouter::ClusterType cluster_type() const noexcept = 0;

  std::vector<mysql_harness::TCPAddress> metadata_servers();

  void enable_fetch_auth_metadata() { auth_metadata_fetch_enabled_ = true; }

  void force_cache_update() { on_refresh_requested(); }

  void check_auth_metadata_timers() const;

  std::pair<bool, MetaData::auth_credentials_t::mapped_type>
  get_rest_user_auth_data(const std::string &user);

  /**
   * Toggle socket acceptors state update on next metadata refresh.
   */
  void handle_sockets_acceptors_on_md_refresh() {
    trigger_acceptor_update_on_next_refresh_ = true;
  }

  bool fetch_whole_topology() const { return fetch_whole_topology_; }

  void fetch_whole_topology(bool val);

 protected:
  /** @brief Refreshes the cache
   *
   */
  virtual bool refresh(bool needs_writable_node) = 0;

  void on_refresh_failed(bool terminated, bool md_servers_reachable = false);
  void on_refresh_succeeded(
      const metadata_cache::metadata_server_t &metadata_server);

  // Called each time the metadata has changed and we need to notify
  // the subscribed observers
  void on_instances_changed(
      const bool md_servers_reachable,
      const metadata_cache::ClusterTopology &cluster_topology,
      uint64_t view_id = 0);

  /**
   * Called when the listening sockets acceptors state should be updated but
   * replicaset instances has not changed (in that case socket acceptors would
   * be handled when calling on_instances_changed).
   */
  void on_handle_sockets_acceptors();

  /**
   * Called on each metadata refresh.
   *
   * @param[in] cluster_nodes_changed Information whether there was a change
   *            in instances reported by metadata refresh.
   * @param[in] cluster_topology current cluster topology
   */
  void on_md_refresh(const bool cluster_nodes_changed,
                     const metadata_cache::ClusterTopology &cluster_topology);

  // Called each time we were requested to refresh the metadata
  void on_refresh_requested();

  // Called each time the metadata refresh completed execution
  void on_refresh_completed();

  // Update rest users authentication data
  bool update_auth_cache();

  // Update current Router attributes in the metadata
  void update_router_attributes();

  // Update Router last_check_in timestamp in the metadata
  void update_router_last_check_in();

  // Stores the current cluster state and topology.
  metadata_cache::ClusterTopology cluster_topology_;

  // identifies the Cluster we work with
  mysqlrouter::TargetCluster target_cluster_;

  // Id of the ClusterSet in case of the ClusterSet setup
  const std::string clusterset_id_;

  // The list of servers that contain the metadata about the managed
  // topology.
  metadata_cache::metadata_servers_list_t metadata_servers_;

  // Metadata TTL configuration.
  metadata_cache::MetadataCacheTTLConfig ttl_config_;

  // SSL options for MySQL connections
  mysqlrouter::SSLOptions ssl_options_;

  // id of the Router in the cluster metadata
  unsigned router_id_;

  struct RestAuthData {
    // Authentication data for the rest users
    MetaData::auth_credentials_t rest_auth_data_;

    std::chrono::system_clock::time_point last_credentials_update_;
  };

  Monitor<RestAuthData> rest_auth_{{}};

  // Authentication data should be fetched only when metadata_cache is used as
  // an authentication backend
  bool auth_metadata_fetch_enabled_{false};

  // Stores the pointer to the transport layer implementation. The transport
  // layer communicates with the servers storing the metadata and fetches the
  // topology information.
  std::shared_ptr<MetaData> meta_data_;

  /** @brief refresh thread facade */
  mysql_harness::MySQLRouterThread refresh_thread_;

  /** @brief notification thread facade */
  mysql_harness::MySQLRouterThread notification_thread_;

  // This mutex is used to ensure that a lookup of the metadata is consistent
  // with the changes in the metadata due to a cache refresh.
  std::mutex cache_refreshing_mutex_;

  // This mutex ensures that a refresh of the servers that contain the metadata
  // is consistent with the use of the server list.
  std::mutex metadata_servers_mutex_;

  // Flag used to terminate the refresh thread.
  std::atomic<bool> terminated_{false};

  bool refresh_requested_{false};

  bool use_cluster_notifications_;

  std::condition_variable refresh_wait_;
  std::mutex refresh_wait_mtx_;

  std::condition_variable refresh_completed_;
  std::mutex refresh_completed_mtx_;

  std::mutex cluster_instances_change_callbacks_mtx_;
  std::mutex acceptor_handler_callbacks_mtx_;
  std::mutex md_refresh_callbacks_mtx_;

  std::set<metadata_cache::ClusterStateListenerInterface *> state_listeners_;
  std::set<metadata_cache::AcceptorUpdateHandlerInterface *>
      acceptor_update_listeners_;
  std::set<metadata_cache::MetadataRefreshListenerInterface *>
      md_refresh_listeners_;

  struct Stats {
    std::chrono::system_clock::time_point last_refresh_failed;
    std::chrono::system_clock::time_point last_refresh_succeeded;
    uint64_t refresh_failed{0};
    uint64_t refresh_succeeded{0};

    std::string last_metadata_server_host;
    uint16_t last_metadata_server_port;
  };

  Monitor<Stats> stats_{{}};

  bool initial_attributes_update_done_{false};
  uint32_t periodic_stats_update_counter_{1};
  std::chrono::steady_clock::time_point last_periodic_stats_update_timestamp_{
      std::chrono::steady_clock::now()};

  bool ready_announced_{false};
  std::atomic<bool> fetch_whole_topology_{false};

  /**
   * Flag indicating if socket acceptors state should be updated on next
   * metadata refresh even if instance information has not changed.
   */
  std::atomic<bool> trigger_acceptor_update_on_next_refresh_{false};

  metadata_cache::RouterAttributes router_attributes_;

  bool needs_initial_attributes_update();
  bool needs_last_check_in_update();
};

std::string to_string(metadata_cache::ServerMode mode);

/** Gets user readable information string about the nodes attributes
 * related to _hidden and _disconnect_existing_sessions_when_hidden tags.
 */
std::string get_hidden_info(const metadata_cache::ManagedInstance &instance);

namespace metadata_cache {

bool operator==(const metadata_cache::ClusterTopology &a,
                const metadata_cache::ClusterTopology &b);
bool operator!=(const metadata_cache::ClusterTopology &a,
                const metadata_cache::ClusterTopology &b);

}  // namespace metadata_cache

#endif  // METADATA_CACHE_METADATA_CACHE_INCLUDED
