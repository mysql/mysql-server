/*
  Copyright (c) 2015, 2024, Oracle and/or its affiliates.

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

#include <system_error>
#include <thread>

#include "destination.h"
#include "mysql/harness/stdx/expected.h"
#include "mysql_routing.h"
#include "mysqlrouter/datatypes.h"
#include "mysqlrouter/metadata_cache.h"
#include "mysqlrouter/routing_export.h"
#include "mysqlrouter/uri.h"
#include "tcp_address.h"

class DestMetadataCacheGroup final
    : public RouteDestination,
      public metadata_cache::ClusterStateListenerInterface,
      public metadata_cache::MetadataRefreshListenerInterface,
      public metadata_cache::AcceptorUpdateHandlerInterface {
 public:
  enum ServerRole { Primary, Secondary, PrimaryAndSecondary };

  /** @brief Constructor */
  DestMetadataCacheGroup(net::io_context &io_ctx_,
                         const std::string &metadata_cache,
                         const routing::RoutingStrategy routing_strategy,
                         const mysqlrouter::URIQuery &query,
                         const Protocol::Type protocol,
                         metadata_cache::MetadataCacheAPIBase *cache_api =
                             metadata_cache::MetadataCacheAPI::instance());

  /** @brief Copy constructor */
  DestMetadataCacheGroup(const DestMetadataCacheGroup &other) = delete;

  /** @brief Move constructor */
  DestMetadataCacheGroup(DestMetadataCacheGroup &&) = delete;

  /** @brief Copy assignment */
  DestMetadataCacheGroup &operator=(const DestMetadataCacheGroup &) = delete;

  /** @brief Move assignment */
  DestMetadataCacheGroup &operator=(DestMetadataCacheGroup &&) = delete;

  ~DestMetadataCacheGroup() override;

  void add(const std::string &, uint16_t) override {}
  void add(const mysql_harness::TCPAddress) override {}

  AddrVector get_destinations() const override;

  /** @brief Returns whether there are destination servers
   *
   * The empty() method always returns false for Metadata Cache.
   *
   * Checking whether the Metadata Cache is empty for given destination
   * might be to expensive. We leave this to the get_server() method.
   *
   * @return Always returns False for Metadata Cache destination.
   */
  bool empty() const noexcept override { return false; }

  /** @brief Start the destination
   *
   * It also overwrites parent class' RouteDestination::start(), which launches
   * Quarantine. For Metadata Cache routing, we don't need it.
   *
   * @param env pointer to the PluginFuncEnv object
   */
  void start(const mysql_harness::PluginFuncEnv *env) override;

  Destinations destinations() override;

  ServerRole server_role() const { return server_role_; }

  // get cache-api
  metadata_cache::MetadataCacheAPIBase *cache_api() { return cache_api_; }

  std::optional<Destinations> refresh_destinations(
      const Destinations &dests) override;

  Destinations primary_destinations();

  /**
   * advance the current position in the destination by n.
   */
  void advance(size_t n);

  void handle_sockets_acceptors() override {
    cache_api()->handle_sockets_acceptors_on_md_refresh();
  }

  routing::RoutingStrategy get_strategy() override { return routing_strategy_; }

 private:
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

  /** @brief Gets available destinations from Metadata Cache
   *
   * This method gets the destinations using Metadata Cache information. It uses
   * the `metadata_cache::get_cluster_nodes()` function to get a list of current
   * managed servers. Bool in the returned pair indicates if (in case of the
   * round-robin-with-fallback routing strategy) the returned nodes are the
   * primaries after the fallback (true), regular primaries (false) or
   * secondaries (false).
   *
   */
  std::pair<metadata_cache::cluster_nodes_list_t, bool> get_available(
      const metadata_cache::cluster_nodes_list_t &instances,
      bool for_new_connections = true) const;

  metadata_cache::cluster_nodes_list_t get_available_primaries(
      const metadata_cache::cluster_nodes_list_t &managed_servers) const;

  Destinations balance(
      const metadata_cache::cluster_nodes_list_t &all_replicaset_nodes,
      bool primary_fallback);

  routing::RoutingStrategy routing_strategy_;

  ServerRole server_role_;

  metadata_cache::MetadataCacheAPIBase *cache_api_;

  bool subscribed_for_metadata_cache_changes_{false};

  bool disconnect_on_promoted_to_primary_{false};
  bool disconnect_on_metadata_unavailable_{false};

  void on_instances_change(
      const metadata_cache::ClusterTopology &cluster_topology,
      const bool md_servers_reachable);
  void subscribe_for_metadata_cache_changes();
  void subscribe_for_acceptor_handler();
  void subscribe_for_md_refresh_handler();

  void notify_instances_changed(
      const metadata_cache::ClusterTopology &cluster_topology,
      const bool md_servers_reachable,
      const uint64_t /*view_id*/) noexcept override;

  bool update_socket_acceptor_state(
      const metadata_cache::cluster_nodes_list_t &instances) noexcept override;

  void on_md_refresh(
      const bool instances_changed,
      const metadata_cache::ClusterTopology &cluster_topology) override;

  // MUST take the RouteDestination Mutex
  size_t start_pos_{};
  size_t ro_start_pos_{};
  size_t rw_start_pos_{};
};

ROUTING_EXPORT DestMetadataCacheGroup::ServerRole get_server_role_from_uri(
    const mysqlrouter::URIQuery &uri);

#endif  // ROUTING_DEST_METADATA_CACHE_INCLUDED
