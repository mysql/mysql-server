/*
  Copyright (c) 2015, 2023, Oracle and/or its affiliates.

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

#ifndef ROUTING_DESTINATION_INCLUDED
#define ROUTING_DESTINATION_INCLUDED

#include <atomic>
#include <cstdint>
#include <list>
#include <mutex>
#include <optional>
#include <string>
#include <system_error>
#include <vector>

#include "my_compiler.h"  // MY_ATTRIBUTE
#include "mysql/harness/net_ts/io_context.h"
#include "mysqlrouter/destination.h"
#include "mysqlrouter/destination_status_types.h"
#include "mysqlrouter/routing.h"
#include "protocol/protocol.h"
#include "tcp_address.h"

namespace mysql_harness {
class PluginFuncEnv;
}

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
    std::function<stdx::expected<void, std::error_code>()>;
using StopSocketAcceptorCallback = std::function<void()>;
// First callback argument informs if the instances returned from the metadata
// has changed. Second argument is a list of new instances available after
// md refresh.
using MetadataRefreshCallback =
    std::function<void(const bool, const AllowedNodes &)>;
// Callback argument is a destination we want to check, value returned is
// true if the destination is quarantined, false otherwise.
using QueryQuarantinedDestinationsCallback =
    std::function<bool(const mysql_harness::TCPAddress &)>;

/** @class DestinationNodesStateNotifier
 *
 * Allows the obervers to register for notifications on the change in the state
 * of the destination nodes.
 */
class DestinationNodesStateNotifier {
 public:
  /** @brief Registers the callback for notification on the change in the
   *         state if the destination nodes.
   *
   * @param clb callback that should be called
   * @return identifier of the inserted callback, can be used to unregister
   *         the callback
   */
  AllowedNodesChangeCallbacksListIterator
  register_allowed_nodes_change_callback(
      const AllowedNodesChangedCallback &clb);

  /** @brief Unregisters the callback registered with
   * register_allowed_nodes_change_callback().
   *
   * @param it  iterator returned by the call to
   * register_allowed_nodes_change_callback()
   */
  void unregister_allowed_nodes_change_callback(
      const AllowedNodesChangeCallbacksListIterator &it);

  /**
   * Registers the callback for notification that the routing socket acceptor
   * should accept new connections.
   *
   * @param clb callback that should be called
   */
  void register_start_router_socket_acceptor(
      const StartSocketAcceptorCallback &clb);

  /**
   * Unregisters the callback registered with
   * register_start_router_socket_acceptor().
   */
  void unregister_start_router_socket_acceptor();

  /**
   * Registers the callback for notification that the routing socket acceptor
   * should stop accepting new connections.
   *
   * @param clb callback that should be called
   */
  void register_stop_router_socket_acceptor(
      const StopSocketAcceptorCallback &clb);

  /**
   * Unregisters the callback registered with
   * register_stop_router_socket_acceptor().
   */
  void unregister_stop_router_socket_acceptor();

  /**
   * Registers a callback that is going to be used on metadata refresh
   *
   * @param callback Callback that will be called on each metadata refresh.
   */
  void register_md_refresh_callback(const MetadataRefreshCallback &callback);

  /**
   * Unregisters the callback registered with
   * register_md_refresh_callback().
   */
  void unregister_md_refresh_callback();

  /**
   * Registers a callback that could be used for checking if the provided
   * destination candidate is currently quarantined.
   *
   * @param clb Callback to query unreachable destinations.
   */
  void register_query_quarantined_destinations(
      const QueryQuarantinedDestinationsCallback &clb);

  /**
   * Unregisters the callback registered with
   * register_query_quarantined_destinations().
   */
  void unregister_query_quarantined_destinations();

 protected:
  AllowedNodesChangeCallbacksList allowed_nodes_change_callbacks_;
  MetadataRefreshCallback md_refresh_callback_;
  StartSocketAcceptorCallback start_router_socket_acceptor_callback_;
  StopSocketAcceptorCallback stop_router_socket_acceptor_callback_;
  QueryQuarantinedDestinationsCallback query_quarantined_destinations_callback_;
  mutable std::mutex allowed_nodes_change_callbacks_mtx_;
  mutable std::mutex md_refresh_callback_mtx_;
  mutable std::mutex socket_acceptor_handle_callbacks_mtx;
  mutable std::mutex query_quarantined_destinations_callback_mtx_;
};

/** @class RouteDestination
 * @brief Manage destinations for a Connection Routing
 *
 * This class manages destinations which are used in Connection Routing.
 * A destination is usually a MySQL Server and is stored using the IP
 * or hostname together with the TCP port (defaulting to 3306 for classic
 * protocol or to 33060 for x protocol).
 *
 * RouteDestination is meant to be a base class and used to inherite and
 * create class which change the behavior. For example, the `get_next()`
 * method is usually changed to get the next server in the list.
 */
class RouteDestination : public DestinationNodesStateNotifier {
 public:
  using AddrVector = std::vector<mysql_harness::TCPAddress>;

  /** @brief Default constructor
   *
   * @param io_ctx context for IO operations
   * @param protocol Protocol for the destination, defaults to value returned
   *        by Protocol::get_default()
   */
  RouteDestination(net::io_context &io_ctx,
                   Protocol::Type protocol = Protocol::get_default())
      : io_ctx_(io_ctx), protocol_(protocol) {}

  /** @brief Destructor */
  virtual ~RouteDestination() = default;

  RouteDestination(const RouteDestination &other) = delete;
  RouteDestination(RouteDestination &&other) = delete;
  RouteDestination &operator=(const RouteDestination &other) = delete;
  RouteDestination &operator=(RouteDestination &&other) = delete;

  /** @brief Return our routing strategy
   */
  virtual routing::RoutingStrategy get_strategy() = 0;

  /** @brief Adds a destination
   *
   * Adds a destination using the given address and port number.
   *
   * @param dest destination address
   */
  virtual void add(const mysql_harness::TCPAddress dest);

  /** @overload */
  virtual void add(const std::string &address, uint16_t port);

  /** @brief Removes a destination
   *
   * Removes a destination using the given address and port number.
   *
   * @param address IP or name
   * @param port Port number
   */
  virtual void remove(const std::string &address, uint16_t port);

  /** @brief Gets destination based on address and port
   *
   * Gets destination base on given address and port and returns a pair
   * with the information.
   *
   * Raises std::out_of_range when the combination of address and port
   * is not in the list of destinations.
   *
   * This function can be used to check whether given destination is in
   * the list.
   *
   * @param address IP or name
   * @param port Port number
   * @return an instance of mysql_harness::TCPAddress
   */
  virtual mysql_harness::TCPAddress get(const std::string &address,
                                        uint16_t port);

  /** @brief Removes all destinations
   *
   * Removes all destinations from the list.
   */
  virtual void clear();

  /** @brief Gets the number of destinations
   *
   * Gets the number of destinations currently in the list.
   *
   * @return Number of destinations as size_t
   */
  size_t size() noexcept;

  /** @brief Returns whether there are destinations
   *
   * @return whether the destination is empty
   */
  virtual bool empty() const noexcept { return destinations_.empty(); }

  /** @brief Start the destination threads (if any)
   *
   * @param env pointer to the PluginFuncEnv object
   */
  virtual void start(const mysql_harness::PluginFuncEnv *env);

  AddrVector::iterator begin() { return destinations_.begin(); }

  AddrVector::const_iterator begin() const { return destinations_.begin(); }

  AddrVector::iterator end() { return destinations_.end(); }

  AddrVector::const_iterator end() const { return destinations_.end(); }

  virtual AddrVector get_destinations() const;

  /**
   * get destinations to connect() to.
   *
   * destinations are in order of preference.
   */
  virtual Destinations destinations() = 0;

  /**
   * refresh destinations.
   *
   * should be called after connecting to all destinations failed.
   *
   * @param dests previous destinations.
   *
   * @returns new destinations, if there are any.
   */
  virtual std::optional<Destinations> refresh_destinations(
      const Destinations &dests);

  /**
   * Trigger listening socket acceptors state handler based on the destination
   * type.
   */
  virtual void handle_sockets_acceptors() {}

 protected:
  /** @brief List of destinations */
  AddrVector destinations_;

  /** @brief Mutex for updating destinations and iterator */
  std::mutex mutex_update_;

  net::io_context &io_ctx_;

  /** @brief Protocol for the destination */
  Protocol::Type protocol_;
};

#endif  // ROUTING_DESTINATION_INCLUDED
