/*
  Copyright (c) 2015, 2026, Oracle and/or its affiliates.

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

#ifndef ROUTING_DEST_METADATA_CACHE_INCLUDED
#define ROUTING_DEST_METADATA_CACHE_INCLUDED

#include "destination.h"
#include "mysql/harness/destination.h"
#include "mysql_routing.h"
#include "mysqlrouter/datatypes.h"
#include "mysqlrouter/metadata_cache.h"
#include "mysqlrouter/routing_export.h"
#include "mysqlrouter/uri.h"
#include "routing_guidelines/routing_guidelines.h"

class DestMetadataCacheManager final
    : public DestinationManager,
      public metadata_cache::ClusterStateListenerInterface,
      public metadata_cache::MetadataRefreshListenerInterface,
      public metadata_cache::AcceptorUpdateHandlerInterface {
 public:
  enum ServerRole { Primary, Secondary, PrimaryAndSecondary };

  /** @brief Constructor */
  DestMetadataCacheManager(net::io_context &io_ctx,
                           MySQLRoutingContext &routing_ctx,
                           const std::string &metadata_cache,
                           const mysqlrouter::URIQuery &query,
                           const ServerRole role,
                           metadata_cache::MetadataCacheAPIBase *cache_api =
                               metadata_cache::MetadataCacheAPI::instance());

  /** @brief Copy constructor */
  DestMetadataCacheManager(const DestMetadataCacheManager &other) = delete;

  /** @brief Move constructor */
  DestMetadataCacheManager(DestMetadataCacheManager &&) = delete;

  /** @brief Copy assignment */
  DestMetadataCacheManager &operator=(const DestMetadataCacheManager &) =
      delete;

  /** @brief Move assignment */
  DestMetadataCacheManager &operator=(DestMetadataCacheManager &&) = delete;

  ~DestMetadataCacheManager() override;

  /** @brief Start the destination
   *
   * It also overwrites parent class' DestinationManager::start(), which
   * launches Quarantine. For Metadata Cache routing, we don't need it.
   *
   * @param env pointer to the PluginFuncEnv object
   */
  void start(const mysql_harness::PluginFuncEnv *env) override;

  mysqlrouter::ServerMode purpose() const override {
    return server_role_ == ServerRole::Primary
               ? mysqlrouter::ServerMode::ReadWrite
               : mysqlrouter::ServerMode::ReadOnly;
  }

  stdx::expected<void, std::error_code> init_destinations(
      const routing_guidelines::Session_info &session_info) override;

  // get cache-api
  metadata_cache::MetadataCacheAPIBase *cache_api() { return cache_api_; }

  bool refresh_destinations(
      const routing_guidelines::Session_info &session_info) override;

  void handle_sockets_acceptors() override {
    cache_api()->handle_sockets_acceptors_on_md_refresh();
  }

  std::unique_ptr<Destination> get_next_destination(
      const routing_guidelines::Session_info &session_info) override;

  std::vector<mysql_harness::Destination> get_destination_candidates()
      const override;

  void connect_status(std::error_code ec) override;

  bool has_read_write() const override { return has_read_write_; }
  bool has_read_only() const override { return has_read_only_; }

  /**
   * Update routing guidelines engine with a new routing guideline.
   *
   * If the new routing guideline is empty then auto-generated guideline is
   * used. If the guidelines engine could not be updated then old guideline is
   * preserved and used.
   *
   * @return List of route names of the routes that have been updated.
   */
  routing_guidelines::Routing_guidelines_engine::RouteChanges
  update_routing_guidelines(const std::string &routing_guidelines_document);

  /**
   * Clear internal state (indexes, last connection status etc). Used when
   * guidelines are updated.
   */
  void clear_internal_state();

  bool is_dynamic() override { return true; }
  std::string get_dynamic_plugin_name() override;

 private:
  /**
   * Resolve hostnames used in routing guidelines document.
   *
   * If hostname could be resolved to multiple addresses (with requested IP
   * version) then only one of the addresses is used, with unspecified order.
   *
   * @param hostnames list of hostnames to be resolved, each host contain
   * information about hostname and IP version which should be used.
   *
   * @return hostname address pairs
   */
  std::unordered_map<std::string, net::ip::address>
  resolve_routing_guidelines_hostnames(
      const std::vector<routing_guidelines::Resolve_host> &hostnames);

  /** Fill each destination group with destination candidates, according to the
   * routing guideline that is being used.
   *
   * For example given the group setting [[d1,d2], [d3]] and
   * d1 containing 127.0.0.1
   * d2 containing 127.0.1.1 127.0.1.2
   * d3 containing 127.0.2.1
   * It will create two destination groups
   * 1) [127.0.0.1, 127.0.1.1, 127.0.1.2]
   * 2) [127.0.2.1]
   * Where group 2) is a backup destination group.
   */
  void prepare_destination_groups();

  /**
   * Change destination group that is currently being used.
   *
   * This happens if there was a connection error and current group could not
   * provide a destination candidate.
   *
   * @retval true successful group change
   * @retval false unsuccessful group change
   */
  bool change_group();

  /**
   * If the routing guideline enables the connection sharing then it validates
   * if the sharing prerequisites are met and it could be used. If not then
   * connection sharing is disabled.
   *
   * @param route_name name of the route that enables connection sharing
   * @param dest destination candidate that is going to be used for connection.
   */
  void validate_current_sharing_settings(std::string_view route_name,
                                         Destination *dest) const;

  std::unique_ptr<Destination> get_next_destination_impl();

  /** Set information if the last connection was successful. */
  void set_last_connect_successful(const bool state);

  /**
   * Get addresses of nodes allowed by the auto-generated routing guideline.
   *
   * Should not be called when user-provided guideline is used as in such case
   * it might be impossible to determine the list upfront (matching criteria
   * might depend on source IP info for example).
   */
  std::vector<routing_guidelines::Server_info>
  get_nodes_allowed_by_routing_guidelines() const;

  /** Get addresses of all nodes in the topology*/
  std::vector<routing_guidelines::Server_info> get_all_nodes() const;

  /** @brief The Metadata Cache to use
   *
   * cache_name_ is the the section key in the configuration of Metadata Cache.
   *
   * For example, given following Metadata Cache configuration, cache_name_ will
   * be set to "ham":
   *
   *     [metadata_cache.ham]
   *     host = metadata.example.com
   *
   */
  const std::string cache_name_;

  /** @brief Query part of the URI given as destination in the configuration
   *
   * For example, given following Metadata Cache configuration:
   *
   *     [routing:metadata_read_only]
   *     ..
   *     destination =
   * metadata_cache:///cluster_name/replicaset_name?role=PRIMARY_AND_SECONDARY
   *
   * The 'role' is part of uri_query_.
   */
  const mysqlrouter::URIQuery uri_query_;

  /** @brief Initializes
   *
   * This method initialized the object. It goes of the URI query information
   * and sets members accordingly.
   */
  void init();

  /** Get destination candidates details from the given topology
   *
   * @param cluster_topology topology of the cluster
   * @param drop_all_hidden shoud the hidden nodes be included in the result
   *
   * @return list of destination candidates
   */
  std::vector<routing_guidelines::Server_info> get_nodes_from_topology(
      const metadata_cache::ClusterTopology &cluster_topology,
      const bool drop_all_hidden) const;

  /**
   * Get information about nodes available for new connections.
   */
  std::vector<routing_guidelines::Server_info> get_new_connection_nodes() const;

  /**
   * Get information about nodes available for existing connections.
   */
  std::vector<routing_guidelines::Server_info> get_old_connection_nodes() const;

  /**
   * Get a destination candidate that was already selected by the Destination
   * Manager, this will not balance destinations or change the Destination
   * Manager internal state.
   */
  std::unique_ptr<Destination> get_last_used_destination() const override {
    return std::make_unique<Destination>(destination_);
  }

  ServerRole server_role_;

  metadata_cache::MetadataCacheAPIBase *cache_api_;

  bool subscribed_for_metadata_cache_changes_{false};

  bool disconnect_on_promoted_to_primary_{false};
  bool disconnect_on_metadata_unavailable_{false};

  void on_instances_change(const bool md_servers_reachable);
  void subscribe_for_metadata_cache_changes();
  void subscribe_for_acceptor_handler();
  void subscribe_for_md_refresh_handler();

  void notify_instances_changed(const bool md_servers_reachable,
                                const uint64_t /*view_id*/) noexcept override;

  bool update_socket_acceptor_state() noexcept override;

  void on_md_refresh(const bool instances_changed) override;

  /**
   * Status of a last connection.
   */
  enum class ConnectionStatus {
    InProgress,
    Failed,
    NotSet,
  };

  /** Routing guideline engine. */
  std::shared_ptr<routing_guidelines::Routing_guidelines_engine>
      routing_guidelines_{nullptr};

  /** @brief Protocol for the destination */
  Protocol::Type protocol_;

  /** Routing strategy that is used within the currently used destination group.
   */
  routing::RoutingStrategy strategy_;

  /** UUID of a last destination returned by get_next_destination(), used in
   * wait for primary failover mechanism. */
  std::string last_server_uuid_;

  /** Guidelines route which is designated by the guidelines engine to handle
   * connection. */
  routing_guidelines::Routing_guidelines_engine::Route_classification
      route_info_;

  /** Destination candidates that are going to be used to create destination
   * groups. */
  std::vector<std::vector<Destination>> destination_candidates_;

  /** Index of the currently used destination group. */
  uint16_t current_destination_group_index_{0};

  /** Index of the current position within a destination group. */
  uint16_t current_group_position_{0};

  // Position of last used destination for each destination groups, used to
  // fairly balance the load in backup destination groups,
  std::map<uint16_t, uint16_t> stored_destination_indexes_;

  /** Information about previous connection status. */
  std::atomic<ConnectionStatus> last_connection_status_{
      ConnectionStatus::NotSet};

  /** How many available destinations are in the currently used destination
   * group. */
  uint16_t available_dests_in_group_{0};

  /** Destination manager contains read-write destination candidates. */
  bool has_read_write_{false};

  /** Destination manager contains read-only destination candidates. */
  bool has_read_only_{false};

  /** Destination thats used for the connection. */
  Destination destination_;
};

ROUTING_EXPORT DestMetadataCacheManager::ServerRole get_server_role_from_uri(
    const mysqlrouter::URIQuery &uri);

#endif  // ROUTING_DEST_METADATA_CACHE_INCLUDED
