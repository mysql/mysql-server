/*
  Copyright (c) 2019, 2026, Oracle and/or its affiliates.

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

#include "mysqlrouter/routing_component.h"

#include <algorithm>
#include <cstring>

#include "mysql/harness/config_option.h"
#include "mysql_routing_base.h"
#include "mysqlrouter/destination_status_component.h"
#include "mysqlrouter/destination_status_types.h"
#include "mysqlrouter/routing.h"
#include "mysqlrouter/supported_router_options.h"

using namespace std::string_literals;

DestinationNodesStateNotifier *
MySQLRoutingAPI::get_destinations_state_notifier() const {
  return r_->destination_manager();
}

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
  return r_->get_context().get_bind_address().hostname();
}

std::chrono::milliseconds MySQLRoutingAPI::get_destination_connect_timeout()
    const {
  return r_->get_context().get_destination_connect_timeout();
}

std::vector<mysql_harness::Destination>
MySQLRoutingAPI::get_destination_candidates() const {
  return r_->get_destination_candidates();
}

MySQLRoutingAPI::SslOptions MySQLRoutingAPI::get_destination_ssl_options()
    const {
  SslOptions result;
  auto &ctxt = r_->get_context();
  auto dest_ssl = ctxt.destination_ssl_config();
  auto ssl_mode = ctxt.dest_ssl_mode();

  if (!dest_ssl) {
    result.ssl_mode = SSL_MODE_DISABLED;
    return result;
  }

  switch (ssl_mode) {
    case SslMode::kDisabled:
      result.ssl_mode = SSL_MODE_DISABLED;
      break;

    case SslMode::kRequired:
      result.ssl_mode = SSL_MODE_REQUIRED;
      break;

    case SslMode::kPreferred:
    case SslMode::kDefault:
    case SslMode::kAsClient:
    case SslMode::kPassthrough:
      result.ssl_mode = SSL_MODE_PREFERRED;
      break;
  }

  switch (dest_ssl->get_verify()) {
    case SslVerify::kDisabled:
      break;
    case SslVerify::kVerifyCa:
      result.ssl_mode = SSL_MODE_VERIFY_CA;
      break;
    case SslVerify::kVerifyIdentity:
      result.ssl_mode = SSL_MODE_VERIFY_IDENTITY;
      break;
  }

  result.ca = dest_ssl->get_ca_file();
  result.capath = dest_ssl->get_ca_path();
  result.crl = dest_ssl->get_crl_file();
  result.crlpath = dest_ssl->get_crl_path();
  result.ssl_cipher = dest_ssl->get_ciphers();
  result.curves = dest_ssl->get_curves();

  return result;
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
  const auto strategy_maybe = r_->get_routing_strategy();
  if (!strategy_maybe) return "";
  return routing::get_routing_strategy_name(*strategy_maybe);
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

void MySQLRoutingAPI::restart_accepting_connections() {
  r_->restart_accepting_connections();
}

void MySQLRoutingAPI::stop_socket_acceptors() {
  r_->stop_socket_acceptors(/*shutting_down*/ false);
}

bool MySQLRoutingAPI::is_running() const { return r_->is_running(); }

void MySQLRoutingComponent::deinit() {
  DestinationStatusComponent::get_instance()
      .stop_unreachable_destinations_quarantine();
  for (auto &route : routes_) {
    if (auto routing_plugin = route.second.lock()) {
      routing_plugin->get_context().shared_quarantine().reset();
    }
  }

  DestinationStatusComponent::get_instance().unregister_quarantine_callbacks();
}

void MySQLRoutingComponent::set_routing_guidelines(
    const std::string &routing_guidelines_document) {
  std::lock_guard<std::mutex> lock(routing_guidelines_mtx_);
  if (!routing_guidelines_) {
    routing_guidelines_ =
        std::make_unique<routing_guidelines::Routing_guidelines_engine>(
            routing_guidelines::Routing_guidelines_engine::create(
                routing_guidelines_document));
  }

  // Default routing guidelines are created based on Router's config, it may be
  // used when user sends an empty guidelines (restore default)
  routing_guidelines_->set_default_routing_guidelines(
      routing_guidelines_document);
}

bool MySQLRoutingComponent::routing_guidelines_initialized() const {
  std::lock_guard<std::mutex> lock(routing_guidelines_mtx_);
  return routing_guidelines_ != nullptr;
}

void MySQLRoutingComponent::register_route(
    const std::string &name, std::shared_ptr<MySQLRoutingBase> srv) {
  auto &quarantine = srv->get_context().shared_quarantine();

  quarantine.on_update([&](const mysql_harness::Destination &dest,
                           bool success) -> bool {
    return DestinationStatusComponent::get_instance().report_connection_result(
        dest, success);
  });

  quarantine.on_is_quarantined([&](const mysql_harness::Destination &dest) {
    return DestinationStatusComponent::get_instance()
        .is_destination_quarantined(dest);
  });

  quarantine.on_stop([&]() {
    DestinationStatusComponent::get_instance()
        .stop_unreachable_destinations_quarantine();
  });

  quarantine.on_refresh([&](const std::string &instance_name,
                            const bool nodes_changed_on_md_refresh,
                            const AllowedNodes &available_destinations) {
    DestinationStatusComponent::get_instance().refresh_destinations_quarantine(
        instance_name, nodes_changed_on_md_refresh, available_destinations);
  });

  DestinationStatusComponent::get_instance().register_route(name);

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

MySQLRoutingConnectionBase *MySQLRoutingComponent::get_connection(
    const std::string &client_endpoint) {
  MySQLRoutingConnectionBase *result = nullptr;

  std::lock_guard<std::mutex> lock(routes_mu_);

  for (const auto &el : routes_) {
    if (auto r = el.second.lock()) {
      if ((result = r->get_connection(client_endpoint))) break;
    }
  }

  return result;
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

const rapidjson::Document &MySQLRoutingComponent::routing_guidelines_document()
    const {
  return routing_guidelines_->get_routing_guidelines_document();
}

rapidjson::Document MySQLRoutingComponent::routing_guidelines_document_schema()
    const {
  rapidjson::Document schema;
  schema.Parse(routing_guidelines_->get_schema());
  return schema;
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
                                  std::string_view option, uint64_t min_value,
                                  uint64_t max_value, uint64_t default_val) {
  std::string conf_str;
  try {
    conf_str = config.get_default(option);
  } catch (const mysql_harness::bad_option &) {
  }

  if (!conf_str.empty()) {
    return mysql_harness::option_as_uint<uint64_t>(
        conf_str, "[DEFAULT]." + std::string(option), min_value, max_value);
  }

  return default_val;
}

void MySQLRoutingComponent::init(const mysql_harness::Config &config) {
  max_total_connections_ =
      get_uint64_config(config, router::options::kMaxTotalConnections, 1,
                        std::numeric_limits<int64_t>::max(),
                        routing::kDefaultMaxTotalConnections);

  QuarantineRoutingCallbacks quarantine_callbacks;

  quarantine_callbacks.on_get_destinations = [&](
      const std::string &route_name) -> auto{
    return this->api(route_name).get_destination_candidates();
  };

  quarantine_callbacks.on_start_acceptors =
      [&](const std::string &route_name) -> void {
    this->api(route_name).restart_accepting_connections();
  };

  quarantine_callbacks.on_stop_acceptors =
      [&](const std::string &route_name) -> void {
    this->api(route_name).stop_socket_acceptors();
  };

  DestinationStatusComponent::get_instance().register_quarantine_callbacks(
      std::move(quarantine_callbacks));
}
