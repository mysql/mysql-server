/*
  Copyright (c) 2015, 2018, Oracle and/or its affiliates. All rights reserved.

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

#ifndef ROUTING_DEST_METADATA_CACHE_INCLUDED
#define ROUTING_DEST_METADATA_CACHE_INCLUDED

#include "destination.h"
#include "mysql_routing.h"
#include "mysqlrouter/metadata_cache.h"
#include "mysqlrouter/uri.h"

#include <thread>

#include "mysql/harness/logging/logging.h"
#include "mysqlrouter/datatypes.h"
#include "tcp_address.h"

class DestMetadataCacheGroup final
    : public RouteDestination,
      public metadata_cache::ReplicasetStateListenerInterface {
 public:
  enum ServerRole { Primary, Secondary, PrimaryAndSecondary };

  /** @brief Constructor */
  DestMetadataCacheGroup(
      const std::string &metadata_cache, const std::string &replicaset,
      const routing::RoutingStrategy routing_strategy,
      const mysqlrouter::URIQuery &query, const Protocol::Type protocol,
      const routing::AccessMode access_mode = routing::AccessMode::kUndefined,
      metadata_cache::MetadataCacheAPIBase *cache_api =
          metadata_cache::MetadataCacheAPI::instance(),
      routing::RoutingSockOpsInterface *routing_sock_ops =
          routing::RoutingSockOps::instance(
              mysql_harness::SocketOperations::instance()));

  /** @brief Copy constructor */
  DestMetadataCacheGroup(const DestMetadataCacheGroup &other) = delete;

  /** @brief Move constructor */
  DestMetadataCacheGroup(DestMetadataCacheGroup &&) = delete;

  /** @brief Copy assignment */
  DestMetadataCacheGroup &operator=(const DestMetadataCacheGroup &) = delete;

  /** @brief Move assignment */
  DestMetadataCacheGroup &operator=(DestMetadataCacheGroup &&) = delete;

  int get_server_socket(
      std::chrono::milliseconds connect_timeout, int *error,
      mysql_harness::TCPAddress *address = nullptr) noexcept override;

  ~DestMetadataCacheGroup() override;

  void add(const std::string &, uint16_t) override {}
  void add(const mysql_harness::TCPAddress) override {}

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

  /** @brief The HA Group which will be used for looking up managed servers */
  const std::string ha_replicaset_;

  /** @brief Query part of the URI given as destination in the configuration
   *
   * For example, given following Metadata Cache configuration:
   *
   *     [routing:metadata_read_only]
   *     ..
   *     destination =
   * metadata_cache:///cluster_name/replicaset_name?allow_primary_reads=yes
   *
   * The 'allow_primary_reads' is part of uri_query_.
   */
  const mysqlrouter::URIQuery uri_query_;

  /** @brief Initializes
   *
   * This method initialized the object. It goes of the URI query information
   * and sets members accordingly.
   */
  void init();

  struct AvailableDestinations {
    AddrVector address;
    std::vector<std::string> id;
  };

  /** @brief Gets available destinations from Metadata Cache
   *
   * This method gets the destinations using Metadata Cache information. It uses
   * the `metadata_cache::lookup_replicaset()` function to get a list of current
   * managed servers.
   *
   */
  AvailableDestinations get_available(
      const metadata_cache::LookupResult &managed_servers,
      bool for_new_connections = true);

  size_t get_next_server(
      const DestMetadataCacheGroup::AvailableDestinations &available);

  size_t current_pos_;

  routing::RoutingStrategy routing_strategy_;

  routing::AccessMode access_mode_;

  ServerRole server_role_;

  metadata_cache::MetadataCacheAPIBase *cache_api_;

  bool subscribed_for_metadata_cache_changes_{false};

  bool disconnect_on_promoted_to_primary_{false};
  bool disconnect_on_metadata_unavailable_{false};

  void on_instances_change(const metadata_cache::LookupResult &instances,
                           const bool md_servers_reachable);
  void subscribe_for_metadata_cache_changes();

  void notify(const metadata_cache::LookupResult &instances,
              const bool md_servers_reachable) noexcept override;
};

#endif  // ROUTING_DEST_METADATA_CACHE_INCLUDED
