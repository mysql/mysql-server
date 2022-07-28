/*
  Copyright (c) 2019, 2022, Oracle and/or its affiliates.

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

#include "mysqlrouter/routing_component.h"
#include "mysql/harness/config_option.h"

#include <cstring>

#include "mysql_routing_base.h"
#include "mysqlrouter/routing.h"

#include <algorithm>
#include <iostream>

using namespace std::string_literals;

int MySQLRoutingAPI::get_max_connections() const {
  return r_->get_max_connections();
}

uint64_t MySQLRoutingAPI::get_max_connect_errors() const {
  return r_->get_context().get_max_connect_errors();
}

std::string MySQLRoutingAPI::get_name() const {
  return r_->get_context().get_name();
}

int MySQLRoutingAPI::get_total_connections() const {
  return r_->get_context().get_handled_routes();
}

int MySQLRoutingAPI::get_active_connections() const {
  return r_->get_context().get_active_routes();
}

std::string MySQLRoutingAPI::get_bind_address() const {
  return r_->get_context().get_bind_address().address();
}

std::chrono::milliseconds MySQLRoutingAPI::get_destination_connect_timeout()
    const {
  return r_->get_context().get_destination_connect_timeout();
}

std::vector<mysql_harness::TCPAddress> MySQLRoutingAPI::get_destinations()
    const {
  return r_->get_destinations();
}

bool MySQLRoutingAPI::is_accepting_connections() const {
  return r_->is_accepting_connections();
}

std::vector<MySQLRoutingAPI::ConnData> MySQLRoutingAPI::get_connections()
    const {
  return r_->get_connections();
}

std::string MySQLRoutingAPI::get_protocol_name() const {
  return r_->get_context().get_protocol() ==
                 BaseProtocol::Type::kClassicProtocol
             ? "classic"
             : "x";
}

std::string MySQLRoutingAPI::get_routing_strategy() const {
  const auto strategy = r_->get_routing_strategy();
  if (strategy == routing::RoutingStrategy::kUndefined) return "";
  return routing::get_routing_strategy_name(strategy);
}

std::string MySQLRoutingAPI::get_mode() const {
  const auto mode = r_->get_mode();
  if (mode == routing::AccessMode::kUndefined) return "";
  return routing::get_access_mode_name(mode);
}

std::string MySQLRoutingAPI::get_destination_replicaset_name() const {
  return "";
}

std::string MySQLRoutingAPI::get_destination_cluster_name() const { return ""; }

std::string MySQLRoutingAPI::get_socket() const {
  return r_->get_context().get_bind_named_socket().str();
}

uint16_t MySQLRoutingAPI::get_bind_port() const {
  return r_->get_context().get_bind_address().port();
}

std::vector<std::string> MySQLRoutingAPI::get_blocked_client_hosts() const {
  return r_->get_context().blocked_endpoints().get_blocked_client_hosts();
}

std::chrono::milliseconds MySQLRoutingAPI::get_client_connect_timeout() const {
  return r_->get_context().get_client_connect_timeout();
}

void MySQLRoutingAPI::start_accepting_connections() {
  r_->start_accepting_connections();
}

void MySQLRoutingAPI::stop_socket_acceptors() { r_->stop_socket_acceptors(); }

bool MySQLRoutingAPI::is_running() const { return r_->is_running(); }

void MySQLRoutingComponent::deinit() {
  routing_common_unreachable_destinations_.stop_quarantine();
  for (auto &route : routes_) {
    if (auto routing_plugin = route.second.lock()) {
      routing_plugin->get_context().shared_quarantine().reset();
    }
  }
}

void MySQLRoutingComponent::init(
    const std::string &name, std::shared_ptr<MySQLRoutingBase> srv,
    std::chrono::seconds quarantine_refresh_interval) {
  auto &quarantine = srv->get_context().shared_quarantine();

  quarantine.on_update([&](const mysql_harness::TCPAddress &addr) {
    routing_common_unreachable_destinations_
        .add_destination_candidate_to_quarantine(addr);
  });

  quarantine.on_is_quarantined([&](const mysql_harness::TCPAddress &addr) {
    return routing_common_unreachable_destinations_.is_quarantined(addr);
  });

  quarantine.on_stop(
      [&]() { routing_common_unreachable_destinations_.stop_quarantine(); });

  quarantine.on_refresh([&](const std::string &instance_name,
                            const bool nodes_changed_on_md_refresh,
                            const AllowedNodes &available_destinations) {
    routing_common_unreachable_destinations_.refresh_quarantine(
        instance_name, nodes_changed_on_md_refresh, available_destinations);
  });

  routing_common_unreachable_destinations_.init(
      name, std::move(quarantine_refresh_interval));

  std::lock_guard<std::mutex> lock(routes_mu_);

  routes_.emplace(name, std::move(srv));
}

void MySQLRoutingComponent::erase(const std::string &name) {
  std::lock_guard<std::mutex> lock(routes_mu_);

  routes_.erase(name);
}

MySQLRoutingComponent &MySQLRoutingComponent::get_instance() {
  static MySQLRoutingComponent instance;

  return instance;
}

std::vector<std::string> MySQLRoutingComponent::route_names() const {
  std::vector<std::string> names;

  for (const auto &el : routes_) {
    names.emplace_back(el.first);
  }

  return names;
}

uint64_t MySQLRoutingComponent::current_total_connections() {
  uint64_t result{};

  std::lock_guard<std::mutex> lock(routes_mu_);

  for (const auto &el : routes_) {
    if (auto r = el.second.lock()) {
      result +=
          r->get_context().info_active_routes_.load(std::memory_order_relaxed);
    }
  }

  return result;
}

MySQLRoutingAPI MySQLRoutingComponent::api(const std::string &name) {
  std::lock_guard<std::mutex> lock(routes_mu_);

  auto it = routes_.find(name);
  if (it == routes_.end()) {
    // not found.
    return {};
  }

  if (auto r = it->second.lock()) {
    return MySQLRoutingAPI(r);
  } else {
    return {};
  }
}

static uint64_t get_uint64_config(const mysql_harness::Config &config,
                                  const std::string &option, uint64_t min_value,
                                  uint64_t max_value, uint64_t default_val) {
  std::string conf_str;
  try {
    conf_str = config.get_default(option);
  } catch (const mysql_harness::bad_option &) {
  }

  if (!conf_str.empty()) {
    return mysql_harness::option_as_uint<uint64_t>(
        conf_str, "[DEFAULT]."s + option, min_value, max_value);
  }

  return default_val;
}

void MySQLRoutingComponent::init(const mysql_harness::Config &config) {
  max_total_connections_ = get_uint64_config(
      config, "max_total_connections", 1, std::numeric_limits<int64_t>::max(),
      kDefaultMaxTotalConnections);
}
