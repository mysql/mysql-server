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

#ifndef ROUTING_DESTINATION_INCLUDED
#define ROUTING_DESTINATION_INCLUDED

#include <cstdint>
#include <list>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include "context.h"
#include "my_compiler.h"  // MY_ATTRIBUTE
#include "mysql/harness/destination.h"
#include "mysql/harness/net_ts/io_context.h"
#include "mysqlrouter/datatypes.h"
#include "mysqlrouter/destination.h"
#include "mysqlrouter/destination_nodes_state_notifier.h"
#include "mysqlrouter/routing.h"
#include "protocol/protocol.h"
#include "routing_guidelines/routing_guidelines.h"

namespace mysql_harness {
class PluginFuncEnv;
}  // namespace mysql_harness

// first argument is the new set of the allowed nodes
// second argument is a set of nodes that can be used for new connections
// third argument is an indication whether we should disconnect existing
// connections (based on disconnect_on_metadata_unavailable setting)
// fourth argument is the description of the condition that triggered the change
// (like 'metadata change' etc.) can be used for logging purposes by the caller
using AllowedNodesChangedCallback =
    std::function<void(const AllowedNodes &, const AllowedNodes &, const bool,
                       const std::string &)>;
// NOTE: this has to be container like std::list that does not invalidate
// iterators when it is modified as we return the iterator to the inserted
// callback to the caller to allow unregistering
using AllowedNodesChangeCallbacksList = std::list<AllowedNodesChangedCallback>;
using AllowedNodesChangeCallbacksListIterator =
    AllowedNodesChangeCallbacksList::iterator;
// Starting a socket acceptor returns a value indicating if the start succeeded.
using StartSocketAcceptorCallback =
    std::function<stdx::expected<void, std::string>()>;
using StopSocketAcceptorCallback = std::function<void()>;
// First callback argument informs if the instances returned from the metadata
// has changed. Second argument is a list of new instances available after
// md refresh.
using MetadataRefreshCallback =
    std::function<void(const bool, const AllowedNodes &)>;

/** @class DestinationManager
 * @brief Manage destinations for a Connection Routing
 *
 * This class manages destinations which are used in Connection Routing.
 * A destination is usually a MySQL Server and is stored using the IP
 * or hostname together with the TCP port (defaulting to 3306 for classic
 * protocol or to 33060 for x protocol).
 */
class DestinationManager : public DestinationNodesStateNotifier {
 public:
  using DestVector = std::vector<mysql_harness::Destination>;

  /** @brief Default constructor
   *
   * @param io_ctx IO context
   * @param routing_ctx Routing context
   */
  DestinationManager(net::io_context &io_ctx, MySQLRoutingContext &routing_ctx)
      : io_ctx_{io_ctx}, routing_ctx_{routing_ctx} {}

  /** @brief Destructor */
  virtual ~DestinationManager() = default;

  DestinationManager(const DestinationManager &other) = delete;
  DestinationManager(DestinationManager &&other) = delete;
  DestinationManager &operator=(const DestinationManager &other) = delete;
  DestinationManager &operator=(DestinationManager &&other) = delete;

  virtual void connect_status(std::error_code ec) = 0;

  /** @brief Start the destination manager
   *
   * @param env pointer to the PluginFuncEnv object
   */
  virtual void start(const mysql_harness::PluginFuncEnv *env) = 0;

  /**
   * Set up destination manager, prepare the destinations.
   *
   * @return error code on failure
   */
  virtual stdx::expected<void, std::error_code> init_destinations(
      const routing_guidelines::Session_info &session_info) = 0;

  virtual mysqlrouter::ServerMode purpose() const {
    return mysqlrouter::ServerMode::Unavailable;
  }

  /**
   * refresh destinations.
   *
   * should be called after connecting to all destinations failed.
   *
   * @retval true refresh suceeded, there are destinations that could be used.
   * @retval false refresh failed, there are no destinations that could be used.
   */
  virtual bool refresh_destinations(
      const routing_guidelines::Session_info &) = 0;

  /**
   * Trigger listening socket acceptors state handler based on the destination
   * type.
   */
  virtual void handle_sockets_acceptors() = 0;

  /**
   * Get destination that should be used for connection attempt.
   *
   * It uses routing strategies and internal information (last used indexes,
   * failed attempt information) for destination selection.
   *
   * @return Destination candidate used for connection attempt.
   */
  virtual std::unique_ptr<Destination> get_next_destination(
      const routing_guidelines::Session_info &) = 0;

  /**
   * Get destination that was selected as a destination candidate.
   */
  virtual std::unique_ptr<Destination> get_last_used_destination() const = 0;

  /**
   * Get addresses of all nodes that are a possible destination candidates.
   */
  virtual std::vector<mysql_harness::Destination> get_destination_candidates()
      const = 0;

  /**
   * Check if routing guidelines uses $.session.rand as a match criterion.
   */
  bool routing_guidelines_session_rand_used() const {
    if (!routing_ctx_.get_routing_guidelines()) return false;
    return routing_ctx_.get_routing_guidelines()->session_rand_used();
  }

  /**
   * Get information about this given Router instance.
   */
  routing_guidelines::Router_info get_router_info() const {
    return routing_ctx_.get_router_info();
  }

  /**
   * Check if there are read-write destinations that could be used.
   */
  virtual bool has_read_write() const = 0;

  /**
   * Check if there are read-only destinations that could be used.
   */
  virtual bool has_read_only() const = 0;

 protected:
  const MySQLRoutingContext &get_routing_context() const {
    return routing_ctx_;
  }

  net::io_context &io_ctx_;
  MySQLRoutingContext &routing_ctx_;
  std::mutex state_mtx_;
  std::error_code last_ec_;
};

#endif  // ROUTING_DESTINATION_INCLUDED
