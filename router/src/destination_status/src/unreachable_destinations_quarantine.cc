/*
  Copyright (c) 2021, 2023, Oracle and/or its affiliates.

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

#include "unreachable_destinations_quarantine.h"

#include "mysql/harness/logging/logging.h"

IMPORT_LOG_FUNCTIONS()

void UnreachableDestinationsQuarantine::register_routing_callbacks(
    QuarantineRoutingCallbacks &&routing_callbacks) {
  std::lock_guard<std::mutex> quarantine_lock{quarantine_mutex_};
  routing_callbacks_ = std::move(routing_callbacks);
}

void UnreachableDestinationsQuarantine::unregister_routing_callbacks() {
  std::lock_guard<std::mutex> quarantine_lock{quarantine_mutex_};
  routing_callbacks_.reset();
}

void UnreachableDestinationsQuarantine::register_route(
    const std::string &route_name) {
  std::lock_guard<std::mutex> l{routing_instances_mutex_};
  routing_instances_.push_back(route_name);
}

void UnreachableDestinationsQuarantine::init(
    std::chrono::seconds quarantine_interval, uint32_t qurantine_threshold) {
  quarantine_interval_ = quarantine_interval;
  quarantine_threshold_ = qurantine_threshold;
}

bool UnreachableDestinationsQuarantine::report_connection_result(
    const mysql_harness::TCPAddress &dest, bool success) {
  bool add_to_quarantine{false};
  {
    std::lock_guard<std::mutex> lock{destination_errors_mutex_};
    if (success) {
      destination_errors_.erase(dest);
    } else {
      if (++destination_errors_[dest] >= quarantine_threshold_) {
        add_to_quarantine = true;
      }
    }
  }
  if (add_to_quarantine) {
    add_destination_candidate_to_quarantine(dest);
  }

  return add_to_quarantine;
}

void UnreachableDestinationsQuarantine::add_destination_candidate_to_quarantine(
    const mysql_harness::TCPAddress &dest) {
  auto referencing_instances = get_referencing_routing_instances(dest);

  {
    std::lock_guard<std::mutex> quarantine_lock{quarantine_mutex_};
    if (stopped_) return;
    auto pos = std::find_if(std::begin(quarantined_destination_candidates_),
                            std::end(quarantined_destination_candidates_),
                            [&dest](const auto &quarantined_dest) {
                              return quarantined_dest->address_ == dest;
                            });
    if (pos != std::end(quarantined_destination_candidates_)) {
      // it is already quarantined, just update the references
      (*pos)->referencing_routing_instances_ = referencing_instances;
      return;
    }

    auto dest_cand = std::make_shared<Unreachable_destination_candidate>(
        &io_ctx_, dest, std::move(referencing_instances), quarantine_interval_,
        /* on delete */
        [&]() {
          std::unique_lock<std::mutex> lk(quarantine_empty_cond_m_);
          quarantined_dest_counter_--;
          quarantine_empty_cond_.notify_all();
        },
        /* on connected to destination */
        [this, dest]() { remove_destination_candidate_from_quarantine(dest); });
    quarantined_dest_counter_++;

    auto &timer = dest_cand->timer_;
    timer.expires_after(quarantine_interval_);
    timer.async_wait([this, dest](const std::error_code &ec) {
      quarantine_handler(ec, dest);
    });

    quarantined_destination_candidates_.push_back(std::move(dest_cand));
  }

  stop_socket_acceptors_on_all_nodes_quarantined();
}

void UnreachableDestinationsQuarantine::
    remove_destination_candidate_from_quarantine(
        const mysql_harness::TCPAddress &dest) {
  log_debug(
      "Destination candidate '%s' is available, remove it from quarantine",
      dest.str().c_str());
  {
    std::lock_guard<std::mutex> lock{destination_errors_mutex_};
    destination_errors_.erase(dest);
  }

  std::lock_guard<std::mutex> quarantine_lock{quarantine_mutex_};
  auto pos = std::find_if(std::begin(quarantined_destination_candidates_),
                          std::end(quarantined_destination_candidates_),
                          [&dest](const auto &quarantined_dest) {
                            return quarantined_dest->address_ == dest;
                          });
  if (pos == std::end(quarantined_destination_candidates_)) {
    return;
  }

  auto &routing_instances = (*pos)->referencing_routing_instances_;
  for (const auto &instance_name : routing_instances) {
    routing_callbacks_.on_start_acceptors(instance_name);
  }

  quarantined_destination_candidates_.erase(pos);
}

bool UnreachableDestinationsQuarantine::is_quarantined(
    const mysql_harness::TCPAddress &dest) {
  std::lock_guard<std::mutex> l{quarantine_mutex_};
  return std::find_if(std::begin(quarantined_destination_candidates_),
                      std::end(quarantined_destination_candidates_),
                      [&dest](const auto &quarantined_dest) {
                        return quarantined_dest->address_ == dest;
                      }) != std::end(quarantined_destination_candidates_);
}

void UnreachableDestinationsQuarantine::refresh_quarantine(
    const std::string &instance_name, const bool nodes_changed_on_md_refresh,
    const AllowedNodes &new_destinations) {
  if (nodes_changed_on_md_refresh) {
    drop_stray_destinations(instance_name, new_destinations);
  }

  update_destinations_state(new_destinations);
}

void UnreachableDestinationsQuarantine::stop_quarantine() {
  {
    std::lock_guard<std::mutex> l{quarantine_mutex_};
    if (stopped_) return;
    log_debug("Clear shared unreachable destinations quarantine list");
    stopped_ = true;
    std::for_each(std::begin(quarantined_destination_candidates_),
                  std::end(quarantined_destination_candidates_),
                  [](auto &dest) {
                    auto &io_ctx = dest->server_sock_.get_executor().context();

                    net::dispatch(io_ctx, [dest]() {
                      dest->server_sock_.cancel();
                      dest->timer_.cancel();
                    });
                  });
    quarantined_destination_candidates_.clear();
  }

  std::unique_lock<std::mutex> lk(quarantine_empty_cond_m_);
  quarantine_empty_cond_.wait(lk,
                              [&] { return quarantined_dest_counter_ == 0; });
}

void UnreachableDestinationsQuarantine::quarantine_handler(
    const std::error_code &ec, const mysql_harness::TCPAddress &dest) {
  // Either there is an quarantine update or we are shutting down.
  if (ec && ec == std::errc::operation_canceled) {
    // leave early at shutdown.
    if (stopped_) return;
  }

  std::shared_ptr<Unreachable_destination_candidate> destination;
  {
    std::lock_guard<std::mutex> l{quarantine_mutex_};
    auto pos = std::find_if(std::begin(quarantined_destination_candidates_),
                            std::end(quarantined_destination_candidates_),
                            [&dest](const auto &quarantined_dest) {
                              return quarantined_dest->address_ == dest;
                            });
    if (pos == std::end(quarantined_destination_candidates_)) return;

    if (ec && ec != std::errc::operation_canceled) {
      // Something went wrong, play it safe and remove the destination
      quarantined_destination_candidates_.erase(pos);
      return;
    }

    destination = *pos;
  }

  auto connect_res = destination->connect();
  if (!connect_res) {
    const auto ec = connect_res.error();
    if ((ec == make_error_condition(std::errc::operation_in_progress) ||
         ec == make_error_condition(std::errc::operation_would_block))) {
      auto &t = destination->timer_;

      t.expires_after(kQuarantinedConnectTimeout);

      t.async_wait([destination](std::error_code ec) {
        if (ec) {
          return;
        }

        destination->connect_timed_out_ = true;
        destination->server_sock_.cancel();
      });

      destination->server_sock_.async_wait(
          net::socket_base::wait_write,
          [this, destination, dest](std::error_code ec) {
            if (ec) {
              if (destination->connect_timed_out_) {
                quarantine_handler({}, dest);
              }
              return;
            }

            destination->timer_.cancel();
            quarantine_handler({}, dest);
          });

      return;
    }

    destination->server_sock_.close();
    auto &timer = destination->timer_;
    destination->func_ =
        Unreachable_destination_candidate::Function::kInitDestination;
    timer.cancel();
    timer.expires_after(quarantine_interval_);
    timer.async_wait([this, dest](const std::error_code &ec) {
      quarantine_handler(ec, dest);
    });
  }
}

void UnreachableDestinationsQuarantine::
    stop_socket_acceptors_on_all_nodes_quarantined() {
  std::lock_guard<std::mutex> plugins_lock{routing_instances_mutex_};
  for (const auto &instance_name : routing_instances_) {
    const auto destinations =
        routing_callbacks_.on_get_destinations(instance_name);

    if (std::all_of(
            std::cbegin(destinations), std::cend(destinations),
            [this](const auto &dest) { return is_quarantined(dest); })) {
      routing_callbacks_.on_stop_acceptors(instance_name);
    }
  }
}

std::vector<std::string>
UnreachableDestinationsQuarantine::get_referencing_routing_instances(
    const mysql_harness::TCPAddress &destination) {
  std::vector<std::string> referencing_instances;
  std::lock_guard<std::mutex> l{routing_instances_mutex_};
  for (const auto &instance_name : routing_instances_) {
    const auto destinations =
        routing_callbacks_.on_get_destinations(instance_name);
    if (std::cend(destinations) != std::find(std::cbegin(destinations),
                                             std::cend(destinations),
                                             destination)) {
      referencing_instances.push_back(instance_name);
    }
  }
  return referencing_instances;
}

void UnreachableDestinationsQuarantine::update_destinations_state(
    const AllowedNodes &destination_list) {
  std::lock_guard<std::mutex> l{quarantine_mutex_};
  for (const auto &destination : destination_list) {
    const auto quarantined_pos =
        std::find_if(std::begin(quarantined_destination_candidates_),
                     std::end(quarantined_destination_candidates_),
                     [&](auto &quarantined_dest) {
                       return quarantined_dest->address_ == destination.address;
                     });
    if (quarantined_pos != std::end(quarantined_destination_candidates_)) {
      (*quarantined_pos)->timer_.cancel();
    }
  }
}

void UnreachableDestinationsQuarantine::drop_stray_destinations(
    const std::string &instance_name,
    const AllowedNodes &routing_new_destinations) {
  std::lock_guard<std::mutex> l{quarantine_mutex_};
  auto quarantined_dest = std::begin(quarantined_destination_candidates_);
  while (quarantined_dest != std::end(quarantined_destination_candidates_)) {
    auto &referencing_instances =
        (*quarantined_dest)->referencing_routing_instances_;
    const auto referencing_routing_pos =
        std::find(std::begin(referencing_instances),
                  std::end(referencing_instances), instance_name);

    // Quarantined destination has a reference to the given routing plugin
    if (referencing_routing_pos != std::end(referencing_instances)) {
      if (std::find_if(std::begin(routing_new_destinations),
                       std::end(routing_new_destinations),
                       [&quarantined_dest](const auto &dest) {
                         return dest.address == (*quarantined_dest)->address_;
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
          (*quarantined_dest)->address_.str().c_str());
      quarantined_dest =
          quarantined_destination_candidates_.erase(quarantined_dest);
    } else {
      quarantined_dest++;
    }
  }
}

UnreachableDestinationsQuarantine::Unreachable_destination_candidate::
    ~Unreachable_destination_candidate() {
  referencing_routing_instances_.clear();
  timer_.cancel();
  if (on_delete_) {
    on_delete_();
  }
}

stdx::expected<void, std::error_code> UnreachableDestinationsQuarantine::
    Unreachable_destination_candidate::connect() {
  switch (func_) {
    case Function::kInitDestination: {
      auto init_res = resolve();
      if (!init_res) return init_res.get_unexpected();

    } break;
    case Function::kConnectFinish: {
      auto connect_res = connect_finish();
      if (!connect_res) return connect_res.get_unexpected();

    } break;
  }

  if (!connected_) {
    auto connect_res = try_connect();
    if (!connect_res) return connect_res.get_unexpected();
  }

  return {};
}

stdx::expected<void, std::error_code> UnreachableDestinationsQuarantine::
    Unreachable_destination_candidate::resolve() {
  net::ip::tcp::resolver resolver(*io_ctx_);
  const auto resolve_res =
      resolver.resolve(address_.address(), std::to_string(address_.port()));

  if (!resolve_res) {
    return resolve_res.get_unexpected();
  }

  endpoints_ = resolve_res.value();

  return init_endpoint();
}

stdx::expected<void, std::error_code> UnreachableDestinationsQuarantine::
    Unreachable_destination_candidate::init_endpoint() {
  endpoints_it_ = endpoints_.begin();

  return connect_init();
}

stdx::expected<void, std::error_code> UnreachableDestinationsQuarantine::
    Unreachable_destination_candidate::connect_init() {
  // close socket if it is already open
  server_sock_.close();
  connect_timed_out_ = false;
  auto endpoint = *endpoints_it_;
  server_endpoint_ = endpoint.endpoint();

  return {};
}

stdx::expected<void, std::error_code> UnreachableDestinationsQuarantine::
    Unreachable_destination_candidate::try_connect() {
  const int socket_flags {
#if defined(SOCK_NONBLOCK)
    // linux|freebsd|sol11.4 allows to set NONBLOCK as part of the socket()
    // call to save the extra syscall
    SOCK_NONBLOCK
#endif
  };

  auto open_res = server_sock_.open(server_endpoint_.protocol(), socket_flags);
  if (!open_res) return open_res.get_unexpected();

  const auto non_block_res = server_sock_.native_non_blocking(true);
  if (!non_block_res) return non_block_res.get_unexpected();

  server_sock_.set_option(net::ip::tcp::no_delay{true});

  const auto connect_res = server_sock_.connect(server_endpoint_);
  if (!connect_res) {
    const auto ec = connect_res.error();
    if (ec == make_error_condition(std::errc::operation_in_progress) ||
        ec == make_error_condition(std::errc::operation_would_block)) {
      // connect in progress, wait for completion.
      func_ = Function::kConnectFinish;
      return connect_res.get_unexpected();
    } else {
      last_ec_ = ec;
      return next_endpoint();
    }
  }
  return connected();
}

stdx::expected<void, std::error_code> UnreachableDestinationsQuarantine::
    Unreachable_destination_candidate::next_endpoint() {
  std::advance(endpoints_it_, 1);

  if (endpoints_it_ != endpoints_.end()) {
    return connect_init();
  } else {
    return stdx::make_unexpected(last_ec_);
  }
}

stdx::expected<void, std::error_code> UnreachableDestinationsQuarantine::
    Unreachable_destination_candidate::connect_finish() {
  if (connect_timed_out_) {
    last_ec_ = make_error_code(std::errc::timed_out);

    return next_endpoint();
  }

  net::socket_base::error sock_err;
  const auto getopt_res = server_sock_.get_option(sock_err);

  if (!getopt_res) {
    last_ec_ = getopt_res.error();
    return next_endpoint();
  }

  if (sock_err.value() != 0) {
    std::error_code ec {
      sock_err.value(),
#if defined(_WIN32)
          std::system_category()
#else
          std::generic_category()
#endif
    };

    last_ec_ = ec;

    return next_endpoint();
  }

  return connected();
}

stdx::expected<void, std::error_code> UnreachableDestinationsQuarantine::
    Unreachable_destination_candidate::connected() {
  connected_ = true;

  if (on_connect_ok_) {
    on_connect_ok_();
  }

  return {};
}
