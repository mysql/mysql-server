/*
  Copyright (c) 2022, 2023, Oracle and/or its affiliates.

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

#ifndef MYSQLROUTER_DESTINATION_STATUS_COMPONENT_INCLUDED
#define MYSQLROUTER_DESTINATION_STATUS_COMPONENT_INCLUDED

#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

#include "mysqlrouter/destination_status_export.h"
#include "mysqlrouter/destination_status_types.h"

#include "tcp_address.h"

class UnreachableDestinationsQuarantine;

/**
 * @class DestinationStatusComponent
 * @brief Shared component representing status of the routing destinations.
 *
 */
class DESTINATION_STATUS_EXPORT DestinationStatusComponent {
 public:
  /**
   * @brief Get the singleton instance object of our class.
   */
  static DestinationStatusComponent &get_instance();

  // disable copy, as we are a single-instance
  DestinationStatusComponent(DestinationStatusComponent const &) = delete;
  void operator=(DestinationStatusComponent const &) = delete;

  // no move either
  DestinationStatusComponent(DestinationStatusComponent &&) = delete;
  void operator=(DestinationStatusComponent &&) = delete;

  ~DestinationStatusComponent();

  /**
   * @brief Initialize the component with the configured options.
   *
   * @param quarantine_interval interval after which the quarantined
   * destinations are checked for availability
   * @param qurantine_threshold number of invalid connect attempts after which
   * the destination is added to the quarantine
   */
  void init(std::chrono::seconds quarantine_interval,
            uint32_t qurantine_threshold);

  /**
   * @brief Register callbacks requied by the quarantine mechanism (start/stop
   * acceptor etc.).
   *
   * @param routing_callbacks object defining the callbacks
   */
  void register_quarantine_callbacks(
      QuarantineRoutingCallbacks &&routing_callbacks);
  /**
   * @brief Unregister callbacks requied by the quarantine mechanism.
   */
  void unregister_quarantine_callbacks();

  /**
   * @brief Register routing instance in the quarantine mechanism.
   *
   * @param name name of the route
   */
  void register_route(const std::string &name);

  /**
   * Register the connection error or success to a given destination.
   *
   * If registering a success it will set the number of reported errors to a
   * given connection to 0.
   *
   * If registering a failure it will increment the number of reported failed
   * connections to the destination. If the number reached the
   * quarantine_threshold the destination will be added to the quarantine. If
   * the destination candidate is not quarantine yet it will starting the async
   * handler for it, otherwise it will just update the referencing plugins list.
   *
   * @param[in] dest Reported destination address.
   * @param[in] success Indicates if the reported connection result is success
   * of failure.
   *
   * @returns true if the destination got added to the quarantine, false
   * otherwise
   */
  bool report_connection_result(const mysql_harness::TCPAddress &dest,
                                bool success);

  /**
   * Query the quarantined destination candidates set and check if the given
   * destination candidate is quarantined.
   *
   * @param[in] dest Destination candidate address.
   * @returns true if the destination candidate is quarantined, false otherwise.
   */
  bool is_destination_quarantined(const mysql_harness::TCPAddress &dest);

  /**
   * Stop all async operations and clear the quarantine list.
   */
  void stop_unreachable_destinations_quarantine();

  /**
   * Refresh the quarantined destination candidates list on metadata refresh.
   *
   * 1) if the destination candidates list got updated we have to go through the
   * quarantined destinations and check if there are still routing plugins that
   * references them.
   * 2) for each destination returned in the metadata (which is available from
   * the md perspective) check if it is still unreachable and should be
   * quarantined.
   *
   * @param[in] instance_name Routing plugin instance name.
   * @param[in] nodes_changed_on_md_refresh Information if the destination
   *            candidates have been updated for the given routing plugin.
   * @param[in] new_destinations List of destination candidates that are
   *            available for the given routing plugin after metadata refresh.
   */
  void refresh_destinations_quarantine(const std::string &instance_name,
                                       const bool nodes_changed_on_md_refresh,
                                       const AllowedNodes &new_destinations);

 private:
  DestinationStatusComponent();

  std::unique_ptr<UnreachableDestinationsQuarantine>
      unreachable_destinations_quarantine_;
};

#endif  // MYSQLROUTER_DESTINATION_STATUS_COMPONENT_INCLUDED
