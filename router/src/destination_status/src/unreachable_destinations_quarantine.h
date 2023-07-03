/*
  Copyright (c) 2021, 2022, Oracle and/or its affiliates.

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

#ifndef MYSQLROUTER_UNREACHABLE_DESTINATIONS_QUARANTINE_INCLUDED
#define MYSQLROUTER_UNREACHABLE_DESTINATIONS_QUARANTINE_INCLUDED

#include <chrono>
#include <map>
#include <mutex>
#include <vector>

#include "mysql/harness/net_ts/internet.h"
#include "mysql/harness/net_ts/io_context.h"
#include "mysql/harness/net_ts/timer.h"
#include "mysqlrouter/destination_status_types.h"
#include "mysqlrouter/io_component.h"

/**
 * Information about unreachable destination candidates that is shared between
 * plugin instances.
 *
 * Quarantined destinations will not be used for
 * routing purposes. Each unreachable destination candidate is periodically
 * probed for availability and removed from the unreachable destination
 * candidate set if it became available.
 */
class UnreachableDestinationsQuarantine {
 public:
  /**
   * Initialize the unreachable destination candidate mechanism.
   *
   * It will set up:
   * - routing plugin instances callbacks used for probing/updating the
   *   unreachable destinations
   * - harness context variable used for starting/stopping the routing listening
   *   sockets
   * - quarantine_refresh_interval Used for unreachable destination candidates
   *   availability checks.
   * @param[in] quarantine_interval The interval in seconds used for checking
   * the health (connectivity) of the quarantined destinations.
   * @param[in] qurantine_threshold Number of consecutive reported failed
   * connections to the destination until it gets quarantined.
   */
  void init(std::chrono::seconds quarantine_interval,
            uint32_t qurantine_threshold);

  void register_routing_callbacks(
      QuarantineRoutingCallbacks &&routing_callbacks);
  void unregister_routing_callbacks();

  void register_route(const std::string &route_name);

  /**
   * @brief
   *
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
   * Remove unreachable destination candidate from quarantine.
   *
   * @param[in] dest Unreachable destination candidate address.
   */
  void remove_destination_candidate_from_quarantine(
      const mysql_harness::TCPAddress &dest);

  /**
   * Query the quarantined destination candidates set and check if the given
   * destination candidate is quarantined.
   *
   * @param[in] dest Destination candidate address.
   * @returns true if the destination candidate is quarantined, false otherwise.
   */
  bool is_quarantined(const mysql_harness::TCPAddress &dest);

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
   * @param[in] available_destinations List of destination candidates that are
   *            available for the given routing plugin after metadata refresh.
   */
  void refresh_quarantine(
      const std::string &instance_name, const bool nodes_changed_on_md_refresh,
      const std::vector<AvailableDestination> &available_destinations);

  /**
   * Stop all async operations and clear the quarantine list.
   */
  void stop_quarantine();

 private:
  /**
   * Async handler responsible of periodic checks for destination candidate
   * availability.
   *
   * @param[in] ec Result of async operation.
   * @param[in] dest Destination candidate address.
   */
  void quarantine_handler(const std::error_code &ec,
                          const mysql_harness::TCPAddress &dest);

  void add_destination_candidate_to_quarantine(
      const mysql_harness::TCPAddress &dest);

  /**
   * Go through all routing instances and check if there are routing plugins
   * which have all destination candidates added to quarantine, if so lets
   * close the listening socket of such routing instances.
   */
  void stop_socket_acceptors_on_all_nodes_quarantined();

  /**
   * For a given destination get names of all routing instances that references
   * it.
   *
   * @param[in] destination Destination candidate address.
   * @returns List of referencing routing instance names
   */
  std::vector<std::string> get_referencing_routing_instances(
      const mysql_harness::TCPAddress &destination);

  /**
   * On metadata refresh we got a destination candidates list that is reported
   * to be available (from the metadata perspective). Go through this list and
   * check if any of the destination candidate is quarantined, if so verify
   * if it is still unreachable and should be kept in quarantine.
   *
   * @param[in] destination_list Destination candidates reported to be
   * available.
   */
  void update_destinations_state(const AllowedNodes &destination_list);

  /**
   * If destination list of a routing instance has changed it is possible that
   * some destinations are no longer referenced by any routing instance. In
   * that case we should scan the quarantine list and remove those destinations.
   *
   * @param[in] instance_name Routing instance name that got destination
   * candidates list update.
   * @param[in] routing_new_destinations List of new destination candidates for
   * the given routing instance.
   */
  void drop_stray_destinations(const std::string &instance_name,
                               const AllowedNodes &routing_new_destinations);

  /**
   * Class representing a single entry (destination) in quarantined destination
   * set.
   *
   * Each destination has its own timer responsible for doing asynchronous
   * availability checks and a list of names of routing instances that currently
   * reference this destination candidate.
   */
  struct Unreachable_destination_candidate {
    Unreachable_destination_candidate(
        net::io_context *io_ctx, const mysql_harness::TCPAddress &addr,
        std::vector<std::string> referencing_instances,
        std::chrono::seconds quarantine_interval,
        std::function<void()> on_delete, std::function<void()> on_connect_ok)
        : io_ctx_{io_ctx},
          address_{addr},
          referencing_routing_instances_{std::move(referencing_instances)},
          quarantine_interval_{quarantine_interval},
          timer_{*io_ctx},
          on_delete_{on_delete},
          on_connect_ok_{on_connect_ok} {}

    ~Unreachable_destination_candidate();

    Unreachable_destination_candidate(Unreachable_destination_candidate &&) =
        default;
    Unreachable_destination_candidate &operator=(
        Unreachable_destination_candidate &&) = default;

    Unreachable_destination_candidate(
        const Unreachable_destination_candidate &) = delete;
    Unreachable_destination_candidate &operator=(
        const Unreachable_destination_candidate &) = delete;

    stdx::expected<void, std::error_code> connect();

    stdx::expected<void, std::error_code> resolve();
    stdx::expected<void, std::error_code> init_endpoint();
    stdx::expected<void, std::error_code> next_endpoint();
    stdx::expected<void, std::error_code> connect_init();
    stdx::expected<void, std::error_code> try_connect();
    stdx::expected<void, std::error_code> connect_finish();
    stdx::expected<void, std::error_code> connected();

    net::io_context *io_ctx_;
    mysql_harness::TCPAddress address_;
    std::vector<std::string> referencing_routing_instances_;
    std::chrono::seconds quarantine_interval_;
    net::steady_timer timer_;

    using server_protocol_type = net::ip::tcp;

    net::ip::tcp::resolver::results_type endpoints_;
    net::ip::tcp::resolver::results_type::iterator endpoints_it_;
    server_protocol_type::socket server_sock_{*io_ctx_};
    server_protocol_type::endpoint server_endpoint_;

    bool connect_timed_out_{false};
    bool connected_{false};

    enum class Function {
      kInitDestination,
      kConnectFinish,
    };

    Function func_{Function::kInitDestination};

    std::error_code last_ec_{
        make_error_code(std::errc::no_such_file_or_directory)};

    std::function<void()> on_delete_;
    std::function<void()> on_connect_ok_;
  };

  std::chrono::milliseconds kQuarantinedConnectTimeout{1000};
  std::chrono::seconds quarantine_interval_{1};
  uint32_t quarantine_threshold_{1};
  net::io_context &io_ctx_ = IoComponent::get_instance().io_context();
  std::mutex quarantine_mutex_;
  std::vector<std::shared_ptr<Unreachable_destination_candidate>>
      quarantined_destination_candidates_;
  std::map<mysql_harness::TCPAddress, uint32_t> destination_errors_;
  std::mutex destination_errors_mutex_;
  std::mutex unreachable_destinations_init_mutex_;
  std::mutex routing_instances_mutex_;
  std::vector<std::string> routing_instances_;
  std::atomic<bool> stopped_{false};

  /** number of quarantined destinations */
  std::atomic<size_t> quarantined_dest_counter_{0};
  std::condition_variable quarantine_empty_cond_;
  std::mutex quarantine_empty_cond_m_;

  QuarantineRoutingCallbacks routing_callbacks_;
};

#endif  // MYSQLROUTER_UNREACHABLE_DESTINATIONS_QUARANTINE_INCLUDED
