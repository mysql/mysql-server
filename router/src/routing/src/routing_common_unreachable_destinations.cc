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

#include "mysqlrouter/routing_common_unreachable_destinations.h"

#include "mysql/harness/logging/logging.h"
#include "mysql_routing.h"

IMPORT_LOG_FUNCTIONS()

static stdx::expected<void, std::error_code> tcp_port_alive(
    net::io_context &io_ctx, const std::string &host, const uint16_t port,
    const std::chrono::milliseconds connect_timeout) {
  net::ip::tcp::resolver resolver(io_ctx);

  const auto resolve_res = resolver.resolve(host, std::to_string(port));
  if (!resolve_res) {
    return resolve_res.get_unexpected();
  }

  std::error_code last_ec{};

  // try all known addresses of the hostname
  for (auto const &resolved : resolve_res.value()) {
    net::ip::tcp::socket sock(io_ctx);

    auto open_res = sock.open(resolved.endpoint().protocol());
    if (!open_res) {
      return open_res.get_unexpected();
    }

    sock.native_non_blocking(true);
    const auto connect_res = sock.connect(resolved.endpoint());

    if (!connect_res) {
      if (connect_res.error() ==
              make_error_condition(std::errc::operation_in_progress) ||
          connect_res.error() ==
              make_error_condition(std::errc::operation_would_block)) {
        std::array<pollfd, 1> pollfds = {{
            {sock.native_handle(), POLLOUT, 0},
        }};

        const auto wait_res = net::impl::poll::poll(
            pollfds.data(), pollfds.size(), connect_timeout);

        if (!wait_res) {
          last_ec = wait_res.error();
        } else {
          net::socket_base::error err;
          const auto status_res = sock.get_option(err);
          if (!status_res) {
            last_ec = status_res.error();
          } else if (err.value() != 0) {
            last_ec = net::impl::socket::make_error_code(err.value());
          } else {
            // success, we can continue
            return {};
          }
        }
      } else {
        last_ec = connect_res.error();
      }
    } else {
      // everything is fine, we are connected
      return {};
    }

    // it failed, try the next address
  }

  return stdx::make_unexpected(last_ec);
}

void RoutingCommonUnreachableDestinations::init(
    const std::string &instance_name,
    std::chrono::seconds quarantine_refresh_interval) {
  {
    std::lock_guard<std::mutex> l{unreachable_destinations_init_mutex_};
    quarantine_interval_ = std::move(quarantine_refresh_interval);
  }

  {
    std::lock_guard<std::mutex> l{routing_instances_mutex_};
    routing_instances_.push_back(instance_name);
  }
}

void RoutingCommonUnreachableDestinations::
    add_destination_candidate_to_quarantine(
        const mysql_harness::TCPAddress &dest) {
  auto referencing_instances = get_referencing_routing_instances(dest);

  {
    std::lock_guard<std::mutex> quarantine_lock{quarantine_mutex_};
    auto pos = std::find_if(std::begin(quarantined_destination_candidates_),
                            std::end(quarantined_destination_candidates_),
                            [&dest](const auto &quarantined_dest) {
                              return quarantined_dest.address_ == dest;
                            });
    if (pos != std::end(quarantined_destination_candidates_)) {
      // it is already quarantined, just update the references
      pos->referencing_routing_instances_ = referencing_instances;
      return;
    }

    net::steady_timer quarantine_timer{io_ctx_};
    quarantine_timer.expires_after(quarantine_interval_);
    quarantine_timer.async_wait([this, dest](const std::error_code &ec) {
      quarantine_handler(ec, dest);
    });
    quarantined_destination_candidates_.emplace_back(
        std::move(dest), std::move(quarantine_timer),
        std::move(referencing_instances));
  }

  stop_socket_acceptors_on_all_nodes_quarantined();
}

bool RoutingCommonUnreachableDestinations::is_quarantined(
    const mysql_harness::TCPAddress &dest) {
  std::lock_guard<std::mutex> l{quarantine_mutex_};
  return std::find_if(std::begin(quarantined_destination_candidates_),
                      std::end(quarantined_destination_candidates_),
                      [&dest](const auto &quarantined_dest) {
                        return quarantined_dest.address_ == dest;
                      }) != std::end(quarantined_destination_candidates_);
}

void RoutingCommonUnreachableDestinations::refresh_quarantine(
    const std::string &instance_name, const bool nodes_changed_on_md_refresh,
    const AllowedNodes &new_destinations) {
  if (nodes_changed_on_md_refresh) {
    drop_stray_destinations(instance_name, new_destinations);
  }

  update_destinations_state(new_destinations);
}

void RoutingCommonUnreachableDestinations::stop_quarantine() {
  log_debug("Clear shared unreachable destinations quarantine list");
  stopped_ = true;
  std::lock_guard<std::mutex> l{quarantine_mutex_};
  std::for_each(std::begin(quarantined_destination_candidates_),
                std::end(quarantined_destination_candidates_),
                [](auto &dest) { dest.timer_.cancel(); });
  quarantined_destination_candidates_.clear();
}

void RoutingCommonUnreachableDestinations::quarantine_handler(
    const std::error_code &ec, const mysql_harness::TCPAddress &dest) {
  // Either there is an quarantine update or we are shutting down.
  if (ec && ec == std::errc::operation_canceled) {
    // leave early at shutdown.
    if (stopped_) return;
  }

  const auto port_alive = tcp_port_alive(io_ctx_, dest.address(), dest.port(),
                                         kQuarantinedConnectTimeout);
  std::lock_guard<std::mutex> l{quarantine_mutex_};
  auto pos = std::find_if(std::begin(quarantined_destination_candidates_),
                          std::end(quarantined_destination_candidates_),
                          [&dest](const auto &quarantined_dest) {
                            return quarantined_dest.address_ == dest;
                          });
  if (pos == std::end(quarantined_destination_candidates_)) return;

  if (ec && ec != std::errc::operation_canceled) {
    // Something went wrong, play it safe and remove the destination
    quarantined_destination_candidates_.erase(pos);
    return;
  }

  if (port_alive) {
    log_debug(
        "Destination candidate '%s' is available, remove it from quarantine",
        pos->address_.str().c_str());

    auto &component = MySQLRoutingComponent::get_instance();
    auto &routing_instances = pos->referencing_routing_instances_;
    for (const auto &instance_name : routing_instances) {
      auto routing_instance = component.api(instance_name);

      routing_instance.start_accepting_connections();
    }
    quarantined_destination_candidates_.erase(pos);
  } else {
    auto &timer = pos->timer_;
    timer.cancel();
    timer.expires_after(quarantine_interval_);
    timer.async_wait([this, dest](const std::error_code &ec) {
      quarantine_handler(ec, dest);
    });
  }
}

void RoutingCommonUnreachableDestinations::
    stop_socket_acceptors_on_all_nodes_quarantined() {
  auto &component = MySQLRoutingComponent::get_instance();

  std::lock_guard<std::mutex> plugins_lock{routing_instances_mutex_};
  for (const auto &instance_name : routing_instances_) {
    auto routing_instance = component.api(instance_name);
    const auto destinations = routing_instance.get_destinations();

    if (std::all_of(
            std::cbegin(destinations), std::cend(destinations),
            [this](const auto &dest) { return is_quarantined(dest); })) {
      routing_instance.stop_socket_acceptors();
    }
  }
}

std::vector<std::string>
RoutingCommonUnreachableDestinations::get_referencing_routing_instances(
    const mysql_harness::TCPAddress &destination) {
  auto &component = MySQLRoutingComponent::get_instance();

  std::vector<std::string> referencing_instances;
  std::lock_guard<std::mutex> l{routing_instances_mutex_};
  for (const auto &instance_name : routing_instances_) {
    const auto destinations = component.api(instance_name).get_destinations();
    if (std::cend(destinations) != std::find(std::cbegin(destinations),
                                             std::cend(destinations),
                                             destination)) {
      referencing_instances.push_back(instance_name);
    }
  }
  return referencing_instances;
}

void RoutingCommonUnreachableDestinations::update_destinations_state(
    const AllowedNodes &destination_list) {
  std::lock_guard<std::mutex> l{quarantine_mutex_};
  for (const auto &destination : destination_list) {
    const auto quarantined_pos =
        std::find_if(std::begin(quarantined_destination_candidates_),
                     std::end(quarantined_destination_candidates_),
                     [&](auto &quarantined_dest) {
                       return quarantined_dest.address_ == destination.address;
                     });
    if (quarantined_pos != std::end(quarantined_destination_candidates_)) {
      quarantined_pos->timer_.cancel();
    }
  }
}

void RoutingCommonUnreachableDestinations::drop_stray_destinations(
    const std::string &instance_name,
    const AllowedNodes &routing_new_destinations) {
  std::lock_guard<std::mutex> l{quarantine_mutex_};
  auto quarantined_dest = std::begin(quarantined_destination_candidates_);
  while (quarantined_dest != std::end(quarantined_destination_candidates_)) {
    auto &referencing_instances =
        quarantined_dest->referencing_routing_instances_;
    const auto referencing_routing_pos =
        std::find(std::begin(referencing_instances),
                  std::end(referencing_instances), instance_name);

    // Quarantined destination has a reference to the given routing plugin
    if (referencing_routing_pos != std::end(referencing_instances)) {
      if (std::find_if(std::begin(routing_new_destinations),
                       std::end(routing_new_destinations),
                       [&quarantined_dest](const auto &dest) {
                         return dest.address == quarantined_dest->address_;
                       }) == std::end(routing_new_destinations)) {
        // Quarantined destination is no longer a destination to the given
        // routing plugin
        referencing_instances.erase(referencing_routing_pos);
      }
    }

    // There is no routing plugin that references this quarantined destination
    // left, it should be removed from the quarantine
    if (referencing_instances.empty()) {
      log_debug(
          "Remove '%s' from quarantine, no plugin is using this destination "
          "candidate",
          quarantined_dest->address_.str().c_str());
      quarantined_dest =
          quarantined_destination_candidates_.erase(quarantined_dest);
    } else {
      quarantined_dest++;
    }
  }
}

RoutingCommonUnreachableDestinations::Unreachable_destination_candidate::
    ~Unreachable_destination_candidate() {
  referencing_routing_instances_.clear();
  timer_.cancel();
}
