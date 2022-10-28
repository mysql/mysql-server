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

#ifndef MYSQLROUTER_METADATA_CACHE_INCLUDED
#define MYSQLROUTER_METADATA_CACHE_INCLUDED

#include "mysqlrouter/metadata_cache_export.h"

#include <atomic>
#include <chrono>
#include <exception>
#include <list>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

#include "my_rapidjson_size_t.h"

#include <rapidjson/document.h>

#include "mysql/harness/stdx/monitor.h"
#include "mysql_router_thread.h"
#include "mysqlrouter/cluster_metadata.h"
#include "mysqlrouter/datatypes.h"
#include "mysqlrouter/metadata.h"
#include "mysqlrouter/metadata_cache_datatypes.h"
#include "mysqlrouter/mysql_session.h"
#include "tcp_address.h"

namespace metadata_cache {
constexpr const uint16_t kDefaultMetadataPort{32275};
constexpr const std::string_view kDefaultMetadataAddress{"127.0.0.1:32275"};
constexpr const std::string_view kDefaultMetadataUser{""};
constexpr const std::string_view kDefaultMetadataPassword{""};
constexpr const std::chrono::milliseconds kDefaultMetadataTTL{500};
constexpr const std::chrono::milliseconds kDefaultAuthCacheTTL{
    std::chrono::seconds{-1}};
constexpr const std::chrono::milliseconds kDefaultAuthCacheRefreshInterval{
    2000};
// blank cluster name means pick the 1st (and only) cluster
constexpr const std::string_view kDefaultMetadataCluster{""};
constexpr const unsigned int kDefaultConnectTimeout{
    mysqlrouter::MySQLSession::kDefaultConnectTimeout};
constexpr const unsigned int kDefaultReadTimeout{
    mysqlrouter::MySQLSession::kDefaultReadTimeout};

constexpr const std::string_view kNodeTagHidden{"_hidden"};
constexpr const std::string_view kNodeTagDisconnectWhenHidden{
    "_disconnect_existing_sessions_when_hidden"};

/** @class metadata_error
 * Class that represents all the exceptions that are thrown while fetching the
 * metadata.
 *
 */
class metadata_error : public std::runtime_error {
 public:
  explicit metadata_error(const std::string &what_arg)
      : std::runtime_error(what_arg) {}
};

/**
 * @brief Abstract class that provides interface for listener on
 *        cluster status changes.
 *
 *        When state of cluster is changed, notify function is called.
 */
class METADATA_CACHE_EXPORT ClusterStateListenerInterface {
 public:
  /**
   * @brief Callback function that is called when state of cluster is
   * changed.
   *
   * @param cluster_topology current cluster topology
   * @param md_servers_reachable true if metadata changed, false if metadata
   * unavailable
   * @param view_id current metadata view_id in case of ReplicaSet cluster
   */
  virtual void notify_instances_changed(const ClusterTopology &cluster_topology,
                                        const bool md_servers_reachable,
                                        const uint64_t view_id) = 0;

  ClusterStateListenerInterface() = default;
  // disable copy as it isn't needed right now. Feel free to enable
  // must be explicitly defined though.
  explicit ClusterStateListenerInterface(
      const ClusterStateListenerInterface &) = delete;
  ClusterStateListenerInterface &operator=(
      const ClusterStateListenerInterface &) = delete;
  virtual ~ClusterStateListenerInterface();
};

/**
 * @brief Abstract class that provides interface for listener on
 *        whether the listening sockets acceptors state should be updated.
 */
class METADATA_CACHE_EXPORT AcceptorUpdateHandlerInterface {
 public:
  /**
   * @brief Callback function that is called when the state of the sockets
   * acceptors is handled during the metadata refresh.
   *
   * @param instances list of the current cluster nodes
   */
  virtual bool update_socket_acceptor_state(
      const metadata_cache::cluster_nodes_list_t &instances) = 0;

  AcceptorUpdateHandlerInterface() = default;

  AcceptorUpdateHandlerInterface(const AcceptorUpdateHandlerInterface &) =
      default;
  AcceptorUpdateHandlerInterface &operator=(
      const AcceptorUpdateHandlerInterface &) = default;

  AcceptorUpdateHandlerInterface(AcceptorUpdateHandlerInterface &&) = default;
  AcceptorUpdateHandlerInterface &operator=(AcceptorUpdateHandlerInterface &&) =
      default;

  virtual ~AcceptorUpdateHandlerInterface() = default;
};

/**
 * Abstract class that provides interface for listener on metadata refresh.
 */
class METADATA_CACHE_EXPORT MetadataRefreshListenerInterface {
 public:
  MetadataRefreshListenerInterface() = default;

  MetadataRefreshListenerInterface(const MetadataRefreshListenerInterface &) =
      default;
  MetadataRefreshListenerInterface(MetadataRefreshListenerInterface &&) =
      default;

  MetadataRefreshListenerInterface &operator=(
      const MetadataRefreshListenerInterface &) = default;
  MetadataRefreshListenerInterface &operator=(
      MetadataRefreshListenerInterface &&) = default;

  /**
   * Callback that is going to be used on each metadata refresh.
   *
   * @param[in] instances_changed Informs if the cluster topology has changed
   * since last md refresh.
   * @param[in] cluster_topology current cluster topology
   */
  virtual void on_md_refresh(const bool instances_changed,
                             const ClusterTopology &cluster_topology) = 0;

  virtual ~MetadataRefreshListenerInterface() = default;
};
/**
 * @brief Abstract class that provides interface for adding and removing
 *        observers on cluster status changes.
 *
 * When state of cluster is changed, then
 * ClusterStateListenerInterface::notify function is called
 * for every registered observer.
 */
class METADATA_CACHE_EXPORT ClusterStateNotifierInterface {
 public:
  /**
   * @brief Register observer that is notified when there is a change in the
   * cluster nodes setup/state discovered.
   *
   * @param listener Observer object that is notified when cluster nodes
   * state is changed.
   *
   * @throw std::runtime_error if metadata cache not initialized
   */
  virtual void add_state_listener(ClusterStateListenerInterface *listener) = 0;

  /**
   * @brief Unregister observer previously registered with add_state_listener()
   *
   * @param listener Observer object that should be unregistered.
   *
   * @throw std::runtime_error if metadata cache not initialized
   */
  virtual void remove_state_listener(
      ClusterStateListenerInterface *listener) = 0;

  ClusterStateNotifierInterface() = default;
  // disable copy as it isn't needed right now. Feel free to enable
  // must be explicitly defined though.
  explicit ClusterStateNotifierInterface(
      const ClusterStateNotifierInterface &) = delete;
  ClusterStateNotifierInterface &operator=(
      const ClusterStateNotifierInterface &) = delete;
  virtual ~ClusterStateNotifierInterface();
};

/**
 * @brief Metadata TTL configuration
 */
struct METADATA_CACHE_EXPORT MetadataCacheTTLConfig {
  // The time to live for the cached data
  std::chrono::milliseconds ttl;

  // auth_cache_ttl TTL of the rest user authentication data
  std::chrono::milliseconds auth_cache_ttl;

  // auth_cache_refresh_interval Refresh rate of the rest user authentication
  // data
  std::chrono::milliseconds auth_cache_refresh_interval;
};

class METADATA_CACHE_EXPORT MetadataCacheAPIBase
    : public ClusterStateNotifierInterface {
 public:
  /** @brief Initialize a MetadataCache object and start caching
   *
   * The metadata_cache::cache_init function will initialize a MetadataCache
   * object using the given arguments and store it globally using the given
   * cache_name.
   *
   * Parameters host, port, user, password are used to setup the connection with
   * the metadata server.
   *
   * Cache name given by cache_name can be empty, but must be unique.
   *
   * The parameters connection_timeout and connection_attempts are used when
   * connected to the metadata server.
   *
   * Throws a std::runtime_error when the cache object was already
   * initialized.
   *
   * @param cluster_type type of the cluster the metadata cache object will
   *                     represent (GR or ReplicaSet)
   * @param router_id id of the router in the cluster metadata
   * @param cluster_type_specific_id id of the ReplicaSet in case of the
   * ReplicaSet, Replication Group name for GR Cluster (if bootstrapped as a
   * single Cluster, empty otherwise)
   * @param clusterset_id UUID of the ClusterSet the Cluster belongs to (if
   * bootstrapped as a ClusterSet, empty otherwise)
   * @param metadata_servers The list of cluster metadata servers
   * @param ttl_config metadata TTL configuration
   * @param ssl_options SSL related options for connection
   * @param target_cluster object identifying the Cluster this operation refers
   * to
   * @param session_config Metadata MySQL session configuration
   * @param router_attributes Router attributes to be registered in the metadata
   * @param thread_stack_size memory in kilobytes allocated for thread's stack
   * @param use_cluster_notifications Flag indicating if the metadata cache
   *                                  should use cluster notifications as an
   *                                  additional trigger for metadata refresh
   *                                  (only available for GR cluster type)
   * @param view_id last known view_id of the cluster metadata (only relevant
   *                for ReplicaSet cluster)
   *
   */
  virtual void cache_init(
      const mysqlrouter::ClusterType cluster_type, const unsigned router_id,
      const std::string &cluster_type_specific_id,
      const std::string &clusterset_id,
      const metadata_servers_list_t &metadata_servers,
      const MetadataCacheTTLConfig &ttl_config,
      const mysqlrouter::SSLOptions &ssl_options,
      const mysqlrouter::TargetCluster &target_cluster,
      const MetadataCacheMySQLSessionConfig &session_config,
      const RouterAttributes &router_attributes,
      size_t thread_stack_size = mysql_harness::kDefaultStackSizeInKiloBytes,
      bool use_cluster_notifications = false, const uint64_t view_id = 0) = 0;

  virtual void instance_name(const std::string &inst_name) = 0;
  virtual std::string instance_name() const = 0;

  virtual bool is_initialized() noexcept = 0;

  virtual bool fetch_whole_topology() const = 0;
  virtual void fetch_whole_topology(bool val) = 0;

  virtual mysqlrouter::ClusterType cluster_type() const = 0;

  /**
   * @brief Start the metadata cache
   */
  virtual void cache_start() = 0;

  /**
   * @brief Teardown the metadata cache
   */
  virtual void cache_stop() noexcept = 0;

  /** @brief Returns list of managed server in a HA cluster
   * * Returns a list of MySQL servers managed by the topology for the given
   * HA cluster.
   *
   * @return List of ManagedInstance objects
   */
  virtual cluster_nodes_list_t get_cluster_nodes() = 0;

  /** @brief Return object containing current cluster topology.
   *
   * @return List of ManagedInstance objects
   */
  virtual ClusterTopology get_cluster_topology() = 0;

  /** @brief Wait until there's a primary member in the cluster
   *
   * To be called when the primary member of a single-primary cluster is down
   * and we want to wait until one becomes elected.
   *
   * @param primary_server_uuid - server_uuid of the PRIMARY that shall be
   * failover from.
   * @param timeout - amount of time to wait for a failover, in seconds
   * @return true if a primary member exists
   */
  virtual bool wait_primary_failover(const std::string &primary_server_uuid,
                                     const std::chrono::seconds &timeout) = 0;

  /**
   * @brief Register observer that is notified when there is a change in the
   * cluster nodes setup/state discovered.
   *
   * @param listener Observer object that is notified when cluster nodes
   * state is changed.
   */
  void add_state_listener(ClusterStateListenerInterface *listener) override = 0;

  /**
   * @brief Unregister observer previously registered with add_state_listener()
   *
   * @param listener Observer object that should be unregistered.
   */
  void remove_state_listener(ClusterStateListenerInterface *listener) override =
      0;

  /**
   * @brief Register observer that is notified when the state of listening
   * socket acceptors should be updated on the next metadata refresh.
   *
   * @param listener Observer object that is notified when replicaset nodes
   * state is changed.
   */
  virtual void add_acceptor_handler_listener(
      AcceptorUpdateHandlerInterface *listener) = 0;

  /**
   * @brief Unregister observer previously registered with
   * add_acceptor_handler_listener()
   *
   * @param listener Observer object that should be unregistered.
   */
  virtual void remove_acceptor_handler_listener(
      AcceptorUpdateHandlerInterface *listener) = 0;

  /**
   * Register observer that is notified when the metadata refresh is triggered.
   *
   * @param listener Observer object that is notified on metadata refresh.
   */
  virtual void add_md_refresh_listener(
      MetadataRefreshListenerInterface *listener) = 0;

  /**
   * @brief Unregister observer previously registered with
   * add_md_refresh_listener()
   *
   * @param listener Observer object that should be unregistered.
   */
  virtual void remove_md_refresh_listener(
      MetadataRefreshListenerInterface *listener) = 0;

  /** @brief Get authentication data (password hash and privileges) for the
   *  given user.
   *
   * @param username - name of the user for which the authentidation data
   *                   is requested
   * @return true and password hash with privileges - authentication data
   * requested for the given user.
   * @return false and empty data set - username is not found or authentication
   * data expired.
   */
  virtual std::pair<bool, std::pair<std::string, rapidjson::Document>>
  get_rest_user_auth_data(const std::string &username) const = 0;

  /**
   * @brief Enable fetching authentication metadata when using metadata_cache
   * http authentication backend.
   */
  virtual void enable_fetch_auth_metadata() = 0;

  /**
   * Force cache update in refresh loop.
   */
  virtual void force_cache_update() = 0;

  /**
   * Check values of auth_cache_ttl and auth_cache_refresh_interval timers.
   *
   * @throw std::invalid_argument for each of the following scenarios:
   * 1. auth_cache_ttl < ttl
   * 2. auth_cache_refresh_interval < ttl
   * 3. auth_cache_refresh_interval > auth_cache_ttl
   */
  virtual void check_auth_metadata_timers() const = 0;

  /**
   * Toggle socket acceptors state update on next metadata refresh.
   */
  virtual void handle_sockets_acceptors_on_md_refresh() = 0;

  MetadataCacheAPIBase() = default;
  // disable copy as it isn't needed right now. Feel free to enable
  // must be explicitly defined though.
  explicit MetadataCacheAPIBase(const MetadataCacheAPIBase &) = delete;
  MetadataCacheAPIBase &operator=(const MetadataCacheAPIBase &) = delete;
  ~MetadataCacheAPIBase() override = default;

  struct RefreshStatus {
    uint64_t refresh_failed;
    uint64_t refresh_succeeded;
    std::chrono::system_clock::time_point last_refresh_succeeded;
    std::chrono::system_clock::time_point last_refresh_failed;

    std::string last_metadata_server_host;
    uint16_t last_metadata_server_port;
  };

  virtual RefreshStatus get_refresh_status() = 0;
  virtual std::string cluster_type_specific_id() const = 0;
  virtual mysqlrouter::TargetCluster target_cluster() const = 0;

  virtual std::chrono::milliseconds ttl() const = 0;

  using metadata_factory_t = std::function<std::shared_ptr<MetaData>(
      mysqlrouter::ClusterType cluster_type,
      const metadata_cache::MetadataCacheMySQLSessionConfig &session_config,
      const mysqlrouter::SSLOptions &ssl_options,
      const bool use_cluster_notifications, unsigned view_id)>;

  virtual void set_instance_factory(metadata_factory_t cb) = 0;
};

// This provides a factory method that returns a pluggable instance
// to the underlying transport layer implementation. The transport
// layer provides the means from which the metadata is
// fetched.

std::shared_ptr<MetaData> metadata_factory_get_instance(
    const mysqlrouter::ClusterType cluster_type,
    const metadata_cache::MetadataCacheMySQLSessionConfig &session_config,
    const mysqlrouter::SSLOptions &ssl_options, const bool use_gr_notifications,
    const unsigned view_id);

class METADATA_CACHE_EXPORT MetadataCacheAPI : public MetadataCacheAPIBase {
 public:
  static MetadataCacheAPIBase *instance();

  void cache_init(const mysqlrouter::ClusterType cluster_type,
                  const unsigned router_id,
                  const std::string &cluster_type_specific_id,
                  const std::string &clusterset_id,
                  const metadata_servers_list_t &metadata_servers,
                  const MetadataCacheTTLConfig &ttl_config,
                  const mysqlrouter::SSLOptions &ssl_options,
                  const mysqlrouter::TargetCluster &target_cluster,
                  const MetadataCacheMySQLSessionConfig &session_config,
                  const RouterAttributes &router_attributes,
                  size_t thread_stack_size, bool use_cluster_notifications,
                  const uint64_t view_id) override;

  mysqlrouter::ClusterType cluster_type() const override;

  void instance_name(const std::string &inst_name) override;
  std::string instance_name() const override;

  std::string cluster_type_specific_id() const override;
  mysqlrouter::TargetCluster target_cluster() const override;
  std::chrono::milliseconds ttl() const override;

  bool is_initialized() noexcept override { return is_initialized_; }
  void cache_start() override;

  void cache_stop() noexcept override;

  cluster_nodes_list_t get_cluster_nodes() override;
  ClusterTopology get_cluster_topology() override;

  bool wait_primary_failover(const std::string &primary_server_uuid,
                             const std::chrono::seconds &timeout) override;

  void add_state_listener(ClusterStateListenerInterface *listener) override;

  void remove_state_listener(ClusterStateListenerInterface *listener) override;

  void add_acceptor_handler_listener(
      AcceptorUpdateHandlerInterface *listener) override;

  void remove_acceptor_handler_listener(
      AcceptorUpdateHandlerInterface *listener) override;

  void add_md_refresh_listener(
      MetadataRefreshListenerInterface *listener) override;

  void remove_md_refresh_listener(
      MetadataRefreshListenerInterface *listener) override;

  RefreshStatus get_refresh_status() override;

  std::pair<bool, std::pair<std::string, rapidjson::Document>>
  get_rest_user_auth_data(const std::string &user) const override;

  void enable_fetch_auth_metadata() override;
  void force_cache_update() override;
  void check_auth_metadata_timers() const override;

  void handle_sockets_acceptors_on_md_refresh() override;

  void set_instance_factory(metadata_factory_t cb) override {
    instance_factory_ = std::move(cb);
  }

  bool fetch_whole_topology() const override;
  void fetch_whole_topology(bool val) override;

 private:
  metadata_factory_t instance_factory_{&metadata_factory_get_instance};

  struct InstData {
    std::string name;
  };
  Monitor<InstData> inst_{{}};

  std::atomic<bool> is_initialized_{false};
  MetadataCacheAPI() = default;
  MetadataCacheAPI(const MetadataCacheAPI &) = delete;
  MetadataCacheAPI &operator=(const MetadataCacheAPI &) = delete;
};

}  // namespace metadata_cache

#endif  // MYSQLROUTER_METADATA_CACHE_INCLUDED
