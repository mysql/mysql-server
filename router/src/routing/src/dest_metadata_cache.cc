/*
  Copyright (c) 2016, 2026, Oracle and/or its affiliates.

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

#include "dest_metadata_cache.h"

#include <algorithm>
#include <cctype>  // toupper
#include <chrono>
#include <iterator>  // advance
#include <memory>
#include <mutex>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>

#include "mysql/harness/destination.h"
#include "mysql/harness/logging/logging.h"
#include "mysql/harness/plugin.h"
#include "mysql/harness/string_utils.h"    // ieq
#include "mysql/harness/utility/string.h"  // join
#include "mysqlrouter/connection_pool.h"
#include "mysqlrouter/connection_pool_component.h"
#include "mysqlrouter/destination.h"
#include "mysqlrouter/metadata_cache.h"
#include "mysqlrouter/routing.h"
#include "routing_guidelines/routing_guidelines.h"

using namespace std::chrono_literals;
using namespace std::string_view_literals;

IMPORT_LOG_FUNCTIONS()

// if client wants a PRIMARY and there's none, we can wait up to this amount of
// seconds until giving up and disconnecting the client
// TODO: possibly this should be made into a configurable option
static const auto kPrimaryFailoverTimeout = 10s;

// we keep the allow_primary_reads on this list even though we no longer support
// it, so that we give more specific error message for it
static constexpr std::array supported_params{
    "role", "allow_primary_reads", "disconnect_on_promoted_to_primary",
    "disconnect_on_metadata_unavailable"};

namespace {

const constexpr auto known_roles = std::to_array<
    std::pair<std::string_view, DestMetadataCacheManager::ServerRole>>({
    {"PRIMARY", DestMetadataCacheManager::ServerRole::Primary},
    {"SECONDARY", DestMetadataCacheManager::ServerRole::Secondary},
    {"PRIMARY_AND_SECONDARY",
     DestMetadataCacheManager::ServerRole::PrimaryAndSecondary},
});

}  // namespace

DestMetadataCacheManager::ServerRole get_server_role_from_uri(
    const mysqlrouter::URIQuery &uri) {
  const auto it = uri.find("role");
  if (it == uri.end()) {
    throw std::runtime_error(
        "Missing 'role' in routing destination specification");
  }

  const std::string name = it->second;
  std::string name_uc;
  name_uc.resize(name.size());
  std::transform(name.begin(), name.end(), name_uc.begin(), ::toupper);

  auto role_it =
      std::find_if(known_roles.begin(), known_roles.end(),
                   [name = name_uc](const auto &p) { return p.first == name; });

  if (role_it == known_roles.end()) {
    std::string valid_names;
    for (auto role : known_roles) {
      if (!valid_names.empty()) {
        valid_names += ", ";
      }

      valid_names += role.first;
    }

    throw std::runtime_error(
        "The role in '?role=" + name +
        "' does not contain one of the valid role names: " + valid_names);
  }

  return role_it->second;
}

namespace {

// throws:
// - runtime_error if invalid value for the option was discovered
// - check_option_allowed() throws std::runtime_error (it is expected to throw
// if the given
//   option is not allowed (because of wrong combination with other params.
//   etc.))
bool get_yes_no_option(const mysqlrouter::URIQuery &uri,
                       const std::string &option_name, const bool defalut_res,
                       const std::function<void()> &check_option_allowed) {
  if (uri.find(option_name) == uri.end()) return defalut_res;

  check_option_allowed();  // this should throw if this option is not allowed
                           // for given configuration

  std::string value_lc = uri.at(option_name);
  std::transform(value_lc.begin(), value_lc.end(), value_lc.begin(), ::tolower);

  if (value_lc == "no")
    return false;
  else if (value_lc == "yes")
    return true;
  else
    throw std::runtime_error("Invalid value for option '" + option_name +
                             "'. Allowed are 'yes' and 'no'");
}

// throws runtime_error if the parameter has wrong value or is not allowed for
// given configuration
bool get_disconnect_on_promoted_to_primary(
    const mysqlrouter::URIQuery &uri,
    const DestMetadataCacheManager::ServerRole &role) {
  const std::string kOptionName = "disconnect_on_promoted_to_primary";
  auto check_option_allowed = [&]() {
    if (role != DestMetadataCacheManager::ServerRole::Secondary) {
      throw std::runtime_error("Option '" + kOptionName +
                               "' is valid only for role=SECONDARY");
    }
  };

  return get_yes_no_option(uri, kOptionName, /*default=*/false,
                           check_option_allowed);
}

// throws runtime_error if the parameter has wrong value or is not allowed for
// given configuration
bool get_disconnect_on_metadata_unavailable(const mysqlrouter::URIQuery &uri) {
  const std::string kOptionName = "disconnect_on_metadata_unavailable";
  auto check_option_allowed = [&]() {};  // always allowed

  return get_yes_no_option(uri, kOptionName, /*default=*/false,
                           check_option_allowed);
}

std::string format(const routing_guidelines::Session_info &session_info,
                   bool extended_session_info) {
  std::string text;
  text.append("router_ip=" + session_info.target_ip);
  text.append(" router_port=" + std::to_string(session_info.target_port));
  text.append(" source_ip=" + session_info.source_ip);
  if (extended_session_info) {
    text.append(" user=" + session_info.user);
    text.append(" schema=" + session_info.schema);
    text.append(" attributes=");

    bool first = true;
    for (const auto &attr : session_info.connect_attrs) {
      if (!first) {
        text.append(",");
      } else {
        first = false;
      }
      text.append(attr.first + "=" + attr.second);
    }
  }
  return text;
}

mysql_harness::TcpDestination addr_from_instance(
    const routing_guidelines::Server_info &instance,
    const Protocol::Type protocol) {
  const auto port = protocol == Protocol::Type::kClassicProtocol
                        ? instance.port
                        : instance.port_x;
  return mysql_harness::TcpDestination(instance.address, port);
}

std::string print_destination_candidates(
    const std::vector<std::vector<Destination>> &destination_candidates,
    const Protocol::Type protocol) {
  std::string result{"["};
  for (const auto &group : destination_candidates) {
    result.append("[");
    for (const auto [i, group_pos] : stdx::ranges::views::enumerate(group)) {
      result.append(
          addr_from_instance(group_pos.get_server_info(), protocol).str());
      if (i != group.size() - 1) result.append(", ");
    }
    result.append("]");
  }
  result.append("]");

  return result;
}

}  // namespace

#ifndef DOXYGEN_SHOULD_SKIP_THIS
// doxygen confuses 'const mysqlrouter::URIQuery &query' with
// 'std::map<std::string, std::string>'
DestMetadataCacheManager::DestMetadataCacheManager(
    net::io_context &io_ctx, MySQLRoutingContext &routing_ctx,
    const std::string &metadata_cache, const mysqlrouter::URIQuery &query,
    const ServerRole role, metadata_cache::MetadataCacheAPIBase *cache_api)
    : DestinationManager(io_ctx, routing_ctx),
      cache_name_(metadata_cache),
      uri_query_(query),
      server_role_(role),
      cache_api_(cache_api),
      disconnect_on_promoted_to_primary_(
          get_disconnect_on_promoted_to_primary(query, server_role_)),
      disconnect_on_metadata_unavailable_(
          get_disconnect_on_metadata_unavailable(query)),
      routing_guidelines_{routing_ctx.get_routing_guidelines()},
      protocol_{routing_ctx.get_protocol()} {
  init();
}
#endif

std::vector<routing_guidelines::Server_info>
DestMetadataCacheManager::get_nodes_from_topology(
    const metadata_cache::ClusterTopology &cluster_topology,
    const bool drop_all_hidden) const {
  std::vector<routing_guidelines::Server_info> result;
  const auto &clusterset_name = cluster_topology.name;

  for (const auto &cluster : cluster_topology.clusters_data) {
    const auto cluster_name = cluster.name;

    // In case of standalone Cluster cluster_topology name is empty
    std::string cluster_role{""};
    if (!cluster_topology.name.empty()) {
      cluster_role = cluster.is_primary ? "PRIMARY" : "REPLICA";
    } else {
      cluster_role = routing_guidelines::kUndefinedRole;
    }

    for (const auto &cluster_member : cluster.members) {
      if (cluster_member.ignore ||
          cluster_member.mode == metadata_cache::ServerMode::Unavailable) {
        continue;
      }

      if (cluster_member.hidden &&
          (drop_all_hidden ||
           cluster_member.disconnect_existing_sessions_when_hidden)) {
        continue;
      }

      routing_guidelines::Server_info instance_info;
      instance_info.address = cluster_member.host;
      instance_info.port = cluster_member.port;
      instance_info.port_x = cluster_member.xport;
      instance_info.uuid = cluster_member.mysql_server_uuid;

      if (cluster_member.mode == metadata_cache::ServerMode::ReadWrite) {
        instance_info.member_role = "PRIMARY";
      } else {
        instance_info.member_role =
            cluster_member.type == mysqlrouter::InstanceType::ReadReplica
                ? "READ_REPLICA"
                : "SECONDARY";
      }

      instance_info.tags = cluster_member.tags;
      instance_info.cluster_set_name = clusterset_name;
      instance_info.cluster_role = cluster_role;
      instance_info.cluster_name = cluster_name;
      instance_info.label = cluster_member.label;
      instance_info.cluster_is_invalidated = cluster.is_invalidated;
      instance_info.version = cluster_member.version;

      result.push_back(instance_info);
    }
  }

  return result;
}

std::vector<routing_guidelines::Server_info>
DestMetadataCacheManager::get_old_connection_nodes() const {
  if (!cache_api_->is_initialized()) return {};

  std::vector<routing_guidelines::Server_info> result;
  const auto &cluster_topology = cache_api_->get_cluster_topology();
  auto instances =
      get_nodes_from_topology(cluster_topology, /*drop_all_hidden*/ false);

  bool primary_fallback{false};
  // if we are gathering the nodes for the decision about keeping existing
  // connections we look also at the disconnect_on_promoted_to_primary_ setting
  // if set to 'no' we need to allow primaries for role=SECONDARY
  if (server_role_ == ServerRole::Secondary &&
      !disconnect_on_promoted_to_primary_) {
    primary_fallback = true;
  }
  for (const auto &it : instances) {
    if ((server_role_ == ServerRole::PrimaryAndSecondary) &&
        (it.member_role == "PRIMARY" || it.member_role == "SECONDARY")) {
      result.push_back(it);
      continue;
    }

    if (server_role_ == ServerRole::Secondary &&
        (it.member_role == "SECONDARY" || it.member_role == "READ_REPLICA")) {
      result.push_back(it);
      continue;
    }

    if ((server_role_ == ServerRole::Primary || primary_fallback) &&
        it.member_role == "PRIMARY") {
      result.push_back(it);
      continue;
    }
  }
  return result;
}

std::vector<routing_guidelines::Server_info>
DestMetadataCacheManager::get_nodes_allowed_by_routing_guidelines() const {
  std::vector<routing_guidelines::Server_info> result;

  // This will only match routes which are using $.session.targetPort and
  // $.session.targetIP. It should not be used for user defined guidelines as we
  // cannot guarantee that only such matching rules are used.
  routing_guidelines::Session_info session_info;
  session_info.target_ip = routing_ctx_.get_bind_address().hostname();
  session_info.target_port = routing_ctx_.get_bind_address().port();

  std::vector<
      routing_guidelines::Routing_guidelines_engine::Route::DestinationGroup>
      available_routes;
  const auto &route_info = routing_guidelines_->classify(
      session_info, routing_ctx_.get_router_info());

  if (!route_info.errors.empty()) {
    log_error("Routing guidelines session classification error(s): %s",
              mysql_harness::join(route_info.errors, ", ").c_str());
    return {};
  }

  available_routes = route_info.destination_groups;
  if (available_routes.empty()) return {};

  std::set<std::string> allowed_destination_classes;
  for (const auto &route : available_routes) {
    std::copy(std::cbegin(route.destination_classes),
              std::cend(route.destination_classes),
              std::inserter(allowed_destination_classes,
                            std::begin(allowed_destination_classes)));
  }

  const auto &cluster_topology = cache_api_->get_cluster_topology();
  auto instances =
      get_nodes_from_topology(cluster_topology, /*drop_hidden*/ true);

  const auto context =
      "reachable from " + routing_ctx_.get_bind_address().str() + ": ";

  for (const auto &instance : instances) {
    const auto &instance_classification =
        routing_guidelines_->classify(instance, routing_ctx_.get_router_info());
    if (!instance_classification.errors.empty()) {
      log_error(
          "Routing guidelines classification error(s) when preparing "
          "destinations: %s",
          mysql_harness::join(instance_classification.errors, ", ").c_str());
      return {};
    }
    const auto &destination_classes = instance_classification.class_names;

    for (const auto &destination_class : destination_classes) {
      if (allowed_destination_classes.count(destination_class)) {
        result.emplace_back(instance);
        break;
      }
    }
  }

  return result;
}

std::vector<routing_guidelines::Server_info>
DestMetadataCacheManager::get_all_nodes() const {
  std::vector<routing_guidelines::Server_info> result;
  const auto &cluster_topology = cache_api_->get_cluster_topology();
  return get_nodes_from_topology(cluster_topology, /*drop_hidden*/ true);
}

std::vector<routing_guidelines::Server_info>
DestMetadataCacheManager::get_new_connection_nodes() const {
  if (!routing_guidelines_ || !cache_api_->is_initialized()) return {};
  if (routing_guidelines_->routing_guidelines_updated()) {
    // For user-defined guidelines there might be no direct mapping between a
    // Routing plugin and a guidelines route. Therefore each destination
    // returned by the metadata may be a valid candidate.
    return get_all_nodes();
  } else {
    return get_nodes_allowed_by_routing_guidelines();
  }
}

std::vector<mysql_harness::Destination>
DestMetadataCacheManager::get_destination_candidates() const {
  std::vector<mysql_harness::Destination> result;

  const auto candidates = get_new_connection_nodes();
  for (const auto &dest : candidates) {
    result.push_back(addr_from_instance(dest, protocol_));
  }
  return result;
}

void DestMetadataCacheManager::init() {
  // check if URI does not contain parameters that we don't understand
  for (const auto &uri_param : uri_query_) {
    if (std::find(supported_params.begin(), supported_params.end(),
                  uri_param.first) == supported_params.end()) {
      throw std::runtime_error(
          "Unsupported 'metadata-cache' parameter in URI: '" + uri_param.first +
          "'");
    }
  }

  auto query_part = uri_query_.find("allow_primary_reads");
  if (query_part != uri_query_.end()) {
    throw std::runtime_error(
        "allow_primary_reads is no longer supported, use "
        "role=PRIMARY_AND_SECONDARY instead");
  }
}

void DestMetadataCacheManager::subscribe_for_metadata_cache_changes() {
  cache_api_->add_state_listener(this);
  subscribed_for_metadata_cache_changes_ = true;
}

void DestMetadataCacheManager::subscribe_for_acceptor_handler() {
  cache_api_->add_acceptor_handler_listener(this);
}

void DestMetadataCacheManager::subscribe_for_md_refresh_handler() {
  cache_api_->add_md_refresh_listener(this);
}

std::string DestMetadataCacheManager::get_dynamic_plugin_name() {
  return cache_api_->instance_name();
}

DestMetadataCacheManager::~DestMetadataCacheManager() {
  if (subscribed_for_metadata_cache_changes_) {
    cache_api_->remove_state_listener(this);
    cache_api_->remove_acceptor_handler_listener(this);
    cache_api_->remove_md_refresh_listener(this);
  }
}

// the first round of destinations didn't succeed.
//
// try to fallback.
bool DestMetadataCacheManager::refresh_destinations(
    const routing_guidelines::Session_info &session_info) {
  if (server_role_ != DestMetadataCacheManager::ServerRole::Primary)
    return false;

  const auto failover_successful = cache_api_->wait_primary_failover(
      last_server_uuid_, kPrimaryFailoverTimeout);

  {
    std::lock_guard<std::mutex> lock(state_mtx_);
    current_group_position_ = 0;
    current_destination_group_index_ = 0;
    last_connection_status_ = ConnectionStatus::NotSet;
    if (failover_successful) {
      const auto &route_classification = routing_guidelines_->classify(
          session_info, routing_ctx_.get_router_info());

      if (!route_classification.errors.empty()) {
        log_error(
            "Routing route classification error(s): %s",
            mysql_harness::join(route_classification.errors, ", ").c_str());

        return false;
      }

      route_info_ = route_classification;
      return true;
    }
  }
  return false;
}

void DestMetadataCacheManager::on_instances_change(
    const bool md_servers_reachable) {
  // we got notified that the metadata has changed.
  // If instances is empty then (most likely it is empty)
  // the metadata-cache cannot connect to the metadata-servers
  // In that case we only disconnect clients if
  // the user configured that it should happen
  // (disconnect_on_metadata_unavailable_ == true)
  const bool disconnect =
      md_servers_reachable || disconnect_on_metadata_unavailable_;

  const std::string reason =
      md_servers_reachable ? "metadata change" : "metadata unavailable";

  auto from_instances = [this](const auto &dests) {
    std::vector<AvailableDestination> result;

    for (const auto &dest : dests) {
      metadata_cache::ServerMode mode =
          dest.member_role == "PRIMARY" ? metadata_cache::ServerMode::ReadWrite
                                        : metadata_cache::ServerMode::ReadOnly;

      result.emplace_back(addr_from_instance(dest, protocol_), dest.uuid, mode);
    }

    return result;
  };

  const auto &nodes_for_new_connections =
      from_instances(get_new_connection_nodes());
  const auto &nodes_for_existing_connections =
      from_instances(get_old_connection_nodes());

  std::lock_guard<std::mutex> lock(allowed_nodes_change_callbacks_mtx_);

  // notify all the registered listeners about the list of available nodes
  // change
  for (auto &clb : allowed_nodes_change_callbacks_) {
    clb(nodes_for_existing_connections, nodes_for_new_connections, disconnect,
        reason);
  }
}

void DestMetadataCacheManager::notify_instances_changed(
    const bool md_servers_reachable, const uint64_t /*view_id*/) noexcept {
  on_instances_change(md_servers_reachable);
}

bool DestMetadataCacheManager::update_socket_acceptor_state() noexcept {
  const auto &nodes_for_new_connections = get_new_connection_nodes();

  {
    std::lock_guard<std::mutex> lock(socket_acceptor_handle_callbacks_mtx);
    if (!nodes_for_new_connections.empty() &&
        start_router_socket_acceptor_callback_) {
      const auto &start_acceptor_res = start_router_socket_acceptor_callback_();
      return start_acceptor_res ? true : false;
    }

    if (nodes_for_new_connections.empty() &&
        stop_router_socket_acceptor_callback_) {
      stop_router_socket_acceptor_callback_();
      return true;
    }
  }

  return true;
}

void DestMetadataCacheManager::on_md_refresh(const bool nodes_changed) {
  auto from_instances = [this](const auto &dests) {
    std::vector<AvailableDestination> result;

    for (const auto &dest : dests) {
      metadata_cache::ServerMode mode =
          dest.member_role == "PRIMARY" ? metadata_cache::ServerMode::ReadWrite
                                        : metadata_cache::ServerMode::ReadOnly;

      result.emplace_back(addr_from_instance(dest, protocol_), dest.uuid, mode);
    }
    return result;
  };

  const auto &new_connection_nodes = from_instances(get_new_connection_nodes());
  {
    std::lock_guard<std::mutex> lock(md_refresh_callback_mtx_);
    if (md_refresh_callback_) {
      md_refresh_callback_(nodes_changed, new_connection_nodes);
    }
  }

  if (nodes_changed) clear_internal_state();
}

void DestMetadataCacheManager::start(const mysql_harness::PluginFuncEnv *env) {
  // before using metadata-cache we need to wait for it to be initialized
  while (!cache_api_->is_initialized() && (!env || is_running(env))) {
    std::this_thread::sleep_for(1ms);
  }

  if (!env || is_running(env)) {
    subscribe_for_metadata_cache_changes();
    subscribe_for_acceptor_handler();
    subscribe_for_md_refresh_handler();
  }
}

std::unordered_map<std::string, net::ip::address>
DestMetadataCacheManager::resolve_routing_guidelines_hostnames(
    const std::vector<routing_guidelines::Resolve_host> &addresses) {
  using IP_ver = routing_guidelines::Resolve_host::IP_version;
  std::unordered_map<std::string, net::ip::address> res;
  net::ip::tcp::resolver resolver{io_ctx_};

  for (const auto &host : addresses) {
    const auto resolve_res = resolver.resolve(host.address, "");
    if (!resolve_res) {
      log_warning("Routing guidelines could not resolve: %s",
                  host.address.c_str());
      continue;
    }

    for (const auto &resolved_addr : resolve_res.value()) {
      const auto addr_val = resolved_addr.endpoint().address();
      if ((host.ip_version == IP_ver::IPv4 && addr_val.is_v4()) ||
          (host.ip_version == IP_ver::IPv6 && addr_val.is_v6())) {
        // Check if there are mutiple addresses resolved for one IP version
        auto pos = res.find(host.address);
        if (pos != std::end(res)) {
          if ((pos->second.is_v4() == addr_val.is_v4()) ||
              (pos->second.is_v6() == addr_val.is_v6())) {
            log_debug("Multiple addresses resolved for %s",
                      host.address.c_str());
            break;
          }
        }
        res[host.address] = addr_val;
      }
    }
  }
  return res;
}

void DestMetadataCacheManager::prepare_destination_groups() {
  destination_candidates_.clear();

  const auto &all_nodes =
      get_nodes_from_topology(cache_api_->get_cluster_topology(), true);

  for (const auto &dest_group : route_info_.destination_groups) {
    std::vector<Destination> group;

    for (const auto &destination_class : dest_group.destination_classes) {
      for (auto &destination_candidate : all_nodes) {
        const auto &destination_classification = routing_guidelines_->classify(
            destination_candidate, routing_ctx_.get_router_info());
        if (!destination_classification.errors.empty()) {
          log_error(
              "Routing guidelines classification error when preparing "
              "destinations:\n - %s",
              mysql_harness::join(destination_classification.errors, "\n - ")
                  .c_str());
          return;
        }
        const auto &classes = destination_classification.class_names;

        if (std::cend(classes) != std::find(std::cbegin(classes),
                                            std::cend(classes),
                                            destination_class)) {
          if (mysql_harness::ieq(destination_candidate.member_role,
                                 "PRIMARY")) {
            has_read_write_ = true;
          } else if (mysql_harness::ieq(destination_candidate.member_role,
                                        "SECONDARY") ||
                     mysql_harness::ieq(destination_candidate.member_role,
                                        "READ_REPLICA")) {
            has_read_only_ = true;
          }

          auto &ctx = get_routing_context();
          const auto port = (ctx.get_protocol() == Protocol::Type::kXProtocol)
                                ? destination_candidate.port_x
                                : destination_candidate.port;
          group.emplace_back(
              mysql_harness::TcpDestination{destination_candidate.address,
                                            port},
              destination_candidate, route_info_.route_name,
              route_info_.connection_sharing_allowed);
        }
      }
    }

    destination_candidates_.push_back(std::move(group));
  }

  if (destination_candidates_.empty() ||
      current_destination_group_index_ >= destination_candidates_.size() ||
      destination_candidates_[current_destination_group_index_].empty()) {
    available_dests_in_group_ = 0;
  } else {
    available_dests_in_group_ =
        destination_candidates_[current_destination_group_index_].size();
  }
}

void DestMetadataCacheManager::validate_current_sharing_settings(
    std::string_view route_name, Destination *dest) const {
  if (!dest) return;

  const auto guidelines_sharing =
      dest->guidelines_route_info().connection_sharing_allowed;
  if (!guidelines_sharing || !guidelines_sharing.value()) return;

  auto &ctx = get_routing_context();
  bool sharing_enabled{true};
  if (ctx.source_ssl_mode() == SslMode::kPassthrough) {
    log_info(
        "Route '%s' has connection sharing enabled but it had been ignored, as "
        "client_ssl_mode=PASSTHROUGH.",
        route_name.data());
    sharing_enabled = false;
  } else if (ctx.source_ssl_mode() == SslMode::kPreferred &&
             ctx.dest_ssl_mode() == SslMode::kAsClient) {
    log_info(
        "Route '%s' has connection sharing enabled but it had been ignored, as "
        "client_ssl_mode=PREFERRED and server_ssl_mode=AS_CLIENT.",
        route_name.data());
    sharing_enabled = false;
  }

  if (ctx.get_protocol() == Protocol::Type::kXProtocol) {
    log_info(
        "Route '%s' has connection sharing enabled but it had been ignored, as "
        "protocol=x",
        route_name.data());
    sharing_enabled = false;
  }

  if (!sharing_enabled) dest->disable_connection_sharing();
}

std::unique_ptr<Destination> DestMetadataCacheManager::get_next_destination(
    const routing_guidelines::Session_info &session_info) {
  auto destination = get_next_destination_impl();

  if (destination) {
    last_server_uuid_ = destination->server_uuid();

    const auto &strategy_str = routing::get_routing_strategy_name(strategy_);

    if (log_level_is_handled(mysql_harness::logging::LogLevel::kDebug)) {
      log_debug("RGuidelines: %" PRIu64 ": Will try %s from %s",
                session_info.id, destination->destination().str().c_str(),
                strategy_str.c_str());
    }
    validate_current_sharing_settings(destination->route_name(),
                                      destination.get());
  }

  return destination;
}

bool DestMetadataCacheManager::change_group() {
  current_destination_group_index_++;

  // Skip empty groups
  while (current_destination_group_index_ < destination_candidates_.size() &&
         destination_candidates_[current_destination_group_index_].empty()) {
    current_destination_group_index_++;
  }

  if (current_destination_group_index_ >= destination_candidates_.size() ||
      destination_candidates_[current_destination_group_index_].empty()) {
    log_debug("No more destination groups available");
    current_destination_group_index_ = 0;
    current_group_position_ = 0;
    available_dests_in_group_ = 0;
    return false;
  }

  available_dests_in_group_ =
      destination_candidates_[current_destination_group_index_].size();

  // Each group has its own routing strategy, lets use it
  const auto &dest_group =
      route_info_.destination_groups[current_destination_group_index_];
  const auto strategy_res =
      routing::get_routing_strategy(dest_group.routing_strategy);
  strategy_ = strategy_res.value();

  log_debug("Try switching to destination group %d",
            current_destination_group_index_);

  if (strategy_ == routing::RoutingStrategy::kRoundRobin) {
    // To fairly balance the load in backup destination groups we
    // remember the last used position
    current_group_position_ =
        stored_destination_indexes_[current_destination_group_index_];

    // There are more than one destinations in the group, we can balance the
    // load on other destinations
    if (available_dests_in_group_ > 1) current_group_position_++;

    if (current_group_position_ >=
        destination_candidates_[current_destination_group_index_].size()) {
      current_group_position_ = 0;
    }

    stored_destination_indexes_[current_destination_group_index_] =
        current_group_position_;
  } else {
    current_group_position_ = 0;
  }

  return true;
}

std::unique_ptr<Destination>
DestMetadataCacheManager::get_next_destination_impl() {
  std::lock_guard<std::mutex> lock(state_mtx_);

  // First group is empty, skip it
  if (destination_candidates_[current_destination_group_index_].size() == 0) {
    if (!change_group()) return nullptr;
  }

  if (last_connection_status_ == ConnectionStatus::Failed) {
    current_group_position_++;
    if (current_group_position_ >=
        destination_candidates_[current_destination_group_index_].size()) {
      if (strategy_ == routing::RoutingStrategy::kFirstAvailable) {
        // We have exhausted all possibilities within this group, try to use the
        // next one
        if (!change_group()) return nullptr;
      } else if (strategy_ == routing::RoutingStrategy::kRoundRobin) {
        if (available_dests_in_group_ == 0) {
          // No need to loop around, we tried every dest in this group
          if (!change_group()) return nullptr;
        } else {
          // Loop to the beginning as there are still destinations available
          current_group_position_ = 0;
        }
      }
    }
  } else if (last_connection_status_ == ConnectionStatus::InProgress) {
    if (strategy_ == routing::RoutingStrategy::kFirstAvailable) {
      // previous connection was successful, lets try from the beginning
      current_destination_group_index_ = 0;
      current_group_position_ = 0;

      // First group is empty, go to the first group containing destinations
      if (destination_candidates_[current_destination_group_index_].size() ==
          0) {
        if (!change_group()) return nullptr;
      }
    } else if (strategy_ == routing::RoutingStrategy::kRoundRobin) {
      // Before going to a backup destination group we have to try all groups
      // with higher precedence
      if (current_destination_group_index_ != 0) {
        // previous connection was successful, lets try from the beginning
        current_destination_group_index_ = 0;
        current_group_position_ = 0;
        // If the first group is empty change_group will try to find a group
        // with destinations in it, if there are none we should fail
        if (destination_candidates_[current_destination_group_index_].empty())
          if (!change_group()) return nullptr;
      } else if (last_connection_status_ == ConnectionStatus::InProgress &&
                 available_dests_in_group_ > 1) {
        // Previous connection was ok, there are other destinations in this
        // group so may move forward
        current_group_position_++;
        if (current_group_position_ >=
            destination_candidates_[current_destination_group_index_].size()) {
          current_group_position_ = 0;
        }
      }
    }
  } else if (last_connection_status_ == ConnectionStatus::NotSet) {
    // if the connection status in not set yet then this is the first
    // attempt, do not need to move destination position
    last_connection_status_ = ConnectionStatus::InProgress;
  }

  if (current_destination_group_index_ >= destination_candidates_.size() ||
      current_group_position_ >=
          destination_candidates_[current_destination_group_index_].size()) {
    return nullptr;
  }

  destination_ = destination_candidates_[current_destination_group_index_]
                                        [current_group_position_];
  return std::make_unique<Destination>(destination_);
}

stdx::expected<void, std::error_code>
DestMetadataCacheManager::init_destinations(
    const routing_guidelines::Session_info &session_info) {
  if (!cache_api_->is_initialized()) {
    return stdx::unexpected(
        make_error_code(std::errc::no_such_file_or_directory));
  }
  bool is_debugged =
      log_level_is_handled(mysql_harness::logging::LogLevel::kDebug);

  if (is_debugged) {
    log_debug(
        "Session classification source IP: '%s', target IP: '%s', target port: "
        "'%d'",
        session_info.source_ip.c_str(), session_info.target_ip.c_str(),
        session_info.target_port);

    if (routing_guidelines_->extended_session_info_in_use()) {
      std::stringstream ss;
      for (const auto &attr : session_info.connect_attrs) {
        ss << attr.first << '=' << attr.second << ';';
      }
      log_debug(
          "Session user: '%s', schema: '%s', connection attributes: '%s' ",
          session_info.user.c_str(), session_info.schema.c_str(),
          ss.str().c_str());
    }
  }

  // Get the first matching route from guidelines 'routes' section
  const auto &route_info = routing_guidelines_->classify(
      session_info, routing_ctx_.get_router_info());

  if (!route_info.errors.empty()) {
    log_error("Routing route classification error(s): %s",
              mysql_harness::join(route_info.errors, ", ").c_str());
    return stdx::unexpected(
        make_error_code(std::errc::no_such_file_or_directory));
  }

  if (route_info.route_name.empty()) {
    log_warning("Could not match any route");
    return stdx::unexpected(
        make_error_code(std::errc::no_such_file_or_directory));
  }

  if (is_debugged) {
    log_debug("Incoming session %" PRIu64 ": %s matches route '%s'",
              session_info.id,
              format(session_info,
                     routing_guidelines_->extended_session_info_in_use())
                  .c_str(),
              route_info.route_name.c_str());
  }

  {
    std::lock_guard<std::mutex> lock(state_mtx_);
    route_info_ = std::move(route_info);

    auto &dest_groups = route_info_.destination_groups;
    std::stable_sort(dest_groups.begin(), dest_groups.end(),
                     [](const auto &g1, const auto &g2) {
                       return g1.priority < g2.priority;
                     });
    prepare_destination_groups();

    const auto &dest_group =
        route_info_.destination_groups[current_destination_group_index_];
    const auto strategy_res =
        routing::get_routing_strategy(dest_group.routing_strategy);
    strategy_ = strategy_res.value();

    if (stored_destination_indexes_.empty() ||
        destination_candidates_.size() != stored_destination_indexes_.size()) {
      stored_destination_indexes_.clear();

      const auto dest_group_cnt = destination_candidates_.size();
      for (std::size_t i = 0; i < dest_group_cnt; i++) {
        // Sentinel value, meaning that round robin has not started yet
        stored_destination_indexes_[i] = destination_candidates_[i].size();
      }
    }
  }

  if (is_debugged) {
    log_debug("Destination candidates available: %s",
              print_destination_candidates(destination_candidates_, protocol_)
                  .c_str());
  }

  return {};
}

routing_guidelines::Routing_guidelines_engine::RouteChanges
DestMetadataCacheManager::update_routing_guidelines(
    const std::string &routing_guidelines_document) {
  log_debug("Try to update routing guidelines with: %s",
            routing_guidelines_document.c_str());
  if (routing_guidelines_document.empty() ||
      routing_guidelines_document == "{}") {
    log_info("Restore initial routing guidelines autogenerated from config");
    std::lock_guard l{state_mtx_};
    return routing_guidelines_->restore_default();
  }

  auto new_routing_guidelines =
      routing_guidelines::Routing_guidelines_engine::create(
          routing_guidelines_document);

  const bool has_extended_session_info_support =
      get_routing_context().dest_ssl_mode() == SslMode::kPreferred;
  if (!has_extended_session_info_support &&
      new_routing_guidelines.extended_session_info_in_use()) {
    log_warning(
        "S.session.user, $.session.schema and $.session.connectAttrs are "
        "supported only when ssl_server_mode is set to PREFERRED");
  }

  const auto &hostnames_to_resolve =
      new_routing_guidelines.hostnames_to_resolve();
  new_routing_guidelines.update_resolve_cache(
      resolve_routing_guidelines_hostnames(hostnames_to_resolve));

  return std::invoke(
      [this, new_guide = std::move(new_routing_guidelines)]() mutable {
        std::lock_guard l{state_mtx_};
        return routing_guidelines_->update_routing_guidelines(
            std::move(new_guide),
            /*is_provided_by_user*/ true);
      });
}

void DestMetadataCacheManager::clear_internal_state() {
  std::lock_guard<std::mutex> lock(state_mtx_);
  current_group_position_ = 0;
  current_destination_group_index_ = 0;
  last_connection_status_ = ConnectionStatus::NotSet;

  available_dests_in_group_ =
      destination_candidates_.empty()
          ? 0
          : destination_candidates_[current_destination_group_index_].size();
}

void DestMetadataCacheManager::connect_status(std::error_code ec) {
  std::lock_guard<std::mutex> lock(state_mtx_);
  last_ec_ = ec;
  const bool was_successful = ec == std::error_code{};
  set_last_connect_successful(was_successful);
}

void DestMetadataCacheManager::set_last_connect_successful(
    const bool successful) {
  last_connection_status_ =
      successful ? ConnectionStatus::InProgress : ConnectionStatus::Failed;
  if (successful == false) {
    if (available_dests_in_group_ > 0) available_dests_in_group_--;
  }
}
